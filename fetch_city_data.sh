#!/usr/bin/env bash
#
# fetch_city_data.sh -- obtain modelgen inputs for a list of US cities.
#
# For each requested city this produces, under <outdir>/cities/<slug>/:
#   boundary.shp/.dbf/.shx/.prj  -> modelgen --shape / --dbf
#   boundary.geojson             -> clip polygon (also handy for QGIS)
#   city.osm                     -> modelgen --osm-xml
#   puma.shp/.dbf                -> modelgen --puma-shp / --puma-dbf
#   pums_p.csv / pums_h.csv      -> modelgen --pums-p / --pums-h
#   modelgen.args                -> ready-to-source argument list
#
# Bulk source files are cached under <outdir>/cache/ and shared between all
# cities in the same state, so the per-city cost after the first city in a
# state is a few seconds.
#
# NOT handled: --pop-gis (population-joined GIS). TIGER geometry carries no
# population column; that needs an ACS/decennial join and is out of scope here.
#
# Requires: curl, unzip, awk, ogr2ogr (GDAL), osmium (osmium-tool).

set -euo pipefail

# --------------------------------------------------------------------------
# Defaults
# --------------------------------------------------------------------------
TIGER_YEAR=2025           # newest published TIGER/Line vintage
GAZ_YEAR=2024             # newest published Gazetteer vintage
PUMS_YEAR=2023            # ACS PUMS vintage
PUMS_SPAN="5-Year"        # "5-Year" or "1-Year"
OUTDIR="./data"
CITIES_FILE=""
FORCE=0
SKIP_PUMS=0
SKIP_OSM=0
DRY_RUN=0

UA="cityscape-model-automation/1.0 (stojansz@miamioh.edu)"
CENSUS="https://www2.census.gov"
GEOFABRIK="https://download.geofabrik.de/north-america/us"

# --------------------------------------------------------------------------
# State FIPS -> USPS -> Geofabrik slug.
# Covers the 53 US subregions Geofabrik publishes (50 states + DC + PR + VI).
# --------------------------------------------------------------------------
read -r -d '' STATE_TABLE <<'TABLE' || true
01 AL alabama
02 AK alaska
04 AZ arizona
05 AR arkansas
06 CA california
08 CO colorado
09 CT connecticut
10 DE delaware
11 DC district-of-columbia
12 FL florida
13 GA georgia
15 HI hawaii
16 ID idaho
17 IL illinois
18 IN indiana
19 IA iowa
20 KS kansas
21 KY kentucky
22 LA louisiana
23 ME maine
24 MD maryland
25 MA massachusetts
26 MI michigan
27 MN minnesota
28 MS mississippi
29 MO missouri
30 MT montana
31 NE nebraska
32 NV nevada
33 NH new-hampshire
34 NJ new-jersey
35 NM new-mexico
36 NY new-york
37 NC north-carolina
38 ND north-dakota
39 OH ohio
40 OK oklahoma
41 OR oregon
42 PA pennsylvania
44 RI rhode-island
45 SC south-carolina
46 SD south-dakota
47 TN tennessee
48 TX texas
49 UT utah
50 VT vermont
51 VA virginia
53 WA washington
54 WV west-virginia
55 WI wisconsin
56 WY wyoming
72 PR puerto-rico
78 VI us-virgin-islands
TABLE

# --------------------------------------------------------------------------
# Helpers
# --------------------------------------------------------------------------
log()  { printf '\033[0;36m[%s]\033[0m %s\n' "$(date +%H:%M:%S)" "$*" >&2; }
warn() { printf '\033[0;33m[warn]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[0;31m[fail]\033[0m %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<USAGE
Usage: $(basename "$0") [options] [CITY ...]

Cities may be given as arguments or via -c. Accepted forms:
    "Oxford, OH"      name + USPS state code
    "Chicago, IL"
    3959234           7-digit Census place GEOID (unambiguous, preferred)

Options:
  -c FILE          read cities from FILE, one per line ("#" comments allowed)
  -o DIR           output directory                  (default: $OUTDIR)
  -y YEAR          TIGER/Line vintage                (default: $TIGER_YEAR)
  --gaz-year YEAR  Gazetteer vintage                 (default: $GAZ_YEAR)
  --pums-year YEAR ACS PUMS vintage                  (default: $PUMS_YEAR)
  --pums-span S    "5-Year" or "1-Year"              (default: $PUMS_SPAN)
  --no-pums        skip PUMS/PUMA downloads
  --no-osm         skip the OSM download + extract
  -f, --force      re-download even if cached
  -n, --dry-run    resolve cities and print the plan, download nothing
  -h, --help       show this help

Examples:
  $(basename "$0") "Oxford, OH" "Chicago, IL"
  $(basename "$0") -c cities.txt -o /scratch/cityscape
  $(basename "$0") --no-pums 3959234
USAGE
}

# fetch <url> <dest> -- cached, resumable, atomic download.
fetch() {
    local url=$1 dest=$2
    if [[ -s $dest && $FORCE -eq 0 ]]; then
        log "cached  $(basename "$dest")"
        return 0
    fi
    mkdir -p "$(dirname "$dest")"
    log "GET     $url"
    local resume=()
    [[ -f "$dest.part" ]] && resume=(-C -)
    if ! curl -fL --progress-bar --retry 3 --retry-delay 2 --retry-connrefused \
              -A "$UA" ${resume[@]+"${resume[@]}"} -o "$dest.part" "$url"; then
        rm -f "$dest.part"
        die "download failed: $url"
    fi
    mv "$dest.part" "$dest"
}

# fetch_zip <url> <zipdest> <unpackdir> <sentinel>
fetch_zip() {
    local url=$1 zipdest=$2 dir=$3 sentinel=$4
    if [[ -s "$dir/$sentinel" && $FORCE -eq 0 ]]; then
        log "cached  $sentinel"
        return 0
    fi
    fetch "$url" "$zipdest"
    mkdir -p "$dir"
    unzip -o -q "$zipdest" -d "$dir" || die "unzip failed: $zipdest"
}

# Look up a field from STATE_TABLE. lookup_state <key> <keycol> <outcol>
lookup_state() {
    awk -v key="$1" -v kc="$2" -v oc="$3" '$kc == key { print $oc; exit }' <<<"$STATE_TABLE"
}

slugify() {
    printf '%s' "$1" | tr '[:upper:]' '[:lower:]' | sed -E -e 's/[^a-z0-9]+/-/g' -e 's/^-//' -e 's/-$//'
}

require() {
    command -v "$1" >/dev/null 2>&1 || die "'$1' not found on PATH. $2"
}

# --------------------------------------------------------------------------
# Argument parsing
# --------------------------------------------------------------------------
CITY_ARGS=()
while [[ $# -gt 0 ]]; do
    case $1 in
        -c)           CITIES_FILE=$2; shift 2 ;;
        -o)           OUTDIR=$2; shift 2 ;;
        -y)           TIGER_YEAR=$2; shift 2 ;;
        --gaz-year)   GAZ_YEAR=$2; shift 2 ;;
        --pums-year)  PUMS_YEAR=$2; shift 2 ;;
        --pums-span)  PUMS_SPAN=$2; shift 2 ;;
        --no-pums)    SKIP_PUMS=1; shift ;;
        --no-osm)     SKIP_OSM=1; shift ;;
        -f|--force)   FORCE=1; shift ;;
        -n|--dry-run) DRY_RUN=1; shift ;;
        -h|--help)    usage; exit 0 ;;
        -*)           die "unknown option: $1 (try --help)" ;;
        *)            CITY_ARGS+=("$1"); shift ;;
    esac
done

if [[ -n $CITIES_FILE ]]; then
    [[ -r $CITIES_FILE ]] || die "cannot read cities file: $CITIES_FILE"
    while IFS= read -r line || [[ -n $line ]]; do
        line=${line%%#*}                                  # strip comments
        line=$(printf '%s' "$line" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
        [[ -n $line ]] && CITY_ARGS+=("$line")
    done < "$CITIES_FILE"
fi

[[ ${#CITY_ARGS[@]} -gt 0 ]] || { usage; die "no cities given"; }

require curl "Install it, or use a machine that has it."
require unzip "brew install unzip"
[[ $DRY_RUN -eq 1 ]] || require ogr2ogr "brew install gdal  (or: conda install -c conda-forge gdal)"
if [[ $SKIP_OSM -eq 0 && $DRY_RUN -eq 0 ]]; then
    require osmium "brew install osmium-tool"
fi

CACHE="$OUTDIR/cache"
CITYDIR="$OUTDIR/cities"
mkdir -p "$CACHE" "$CITYDIR"

# --------------------------------------------------------------------------
# Step 1: national Gazetteer -- resolves "Name, ST" -> place GEOID.
# One 1.2 MB file covering all ~32,000 US places.
# --------------------------------------------------------------------------
GAZ_DIR="$CACHE/gazetteer"
GAZ_TXT="$GAZ_DIR/${GAZ_YEAR}_Gaz_place_national.txt"
if [[ $DRY_RUN -eq 0 || ! -s $GAZ_TXT ]]; then
    fetch_zip "$CENSUS/geo/docs/maps-data/data/gazetteer/${GAZ_YEAR}_Gazetteer/${GAZ_YEAR}_Gaz_place_national.zip" \
              "$CACHE/zips/gaz_place_national.zip" \
              "$GAZ_DIR" "${GAZ_YEAR}_Gaz_place_national.txt"
fi
[[ -s $GAZ_TXT ]] || die "gazetteer missing: $GAZ_TXT"

# resolve_city <query> -> "GEOID<TAB>NAMELSAD<TAB>USPS" on stdout, or non-zero.
#
# Bare city names are ambiguous ("Oxford city" exists in 10 states), so a
# name query MUST carry a state. Multiple in-state matches are reported and
# rejected rather than silently guessed.
resolve_city() {
    local query=$1 name state matches count

    if [[ $query =~ ^[0-9]{7}$ ]]; then
        matches=$(awk -F'\t' -v g="$query" '
            NR>1 { gsub(/^[ \t]+|[ \t]+$/,"",$2)
                   if ($2==g) { gsub(/^[ \t]+|[ \t]+$/,"",$4)
                                gsub(/^[ \t]+|[ \t]+$/,"",$1)
                                print $2"\t"$4"\t"$1 } }' "$GAZ_TXT")
    else
        [[ $query == *,* ]] || {
            warn "\"$query\": needs a state, e.g. \"${query}, OH\" (or pass the 7-digit GEOID)"
            return 1
        }
        name=$(printf '%s' "${query%%,*}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
        state=$(printf '%s' "${query##*,}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' \
                | tr '[:lower:]' '[:upper:]')
        matches=$(awk -F'\t' -v want="$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]')" -v st="$state" '
            NR>1 {
                for (i=1;i<=NF;i++) gsub(/^[ \t]+|[ \t]+$/,"",$i)
                if ($1 != st) next
                full = tolower($4)
                base = full
                # drop the trailing legal/statistical designator
                sub(/ (city|town|village|borough|municipality|cdp|township|plantation|comunidad|zona urbana|city and borough|consolidated government|metro government|metropolitan government|unified government|urban county)$/, "", base)
                if (full == want || base == want) print $2"\t"$4"\t"$1
            }' "$GAZ_TXT")
    fi

    count=$(printf '%s' "$matches" | grep -c . || true)
    if [[ $count -eq 0 ]]; then
        warn "\"$query\": no match in the ${GAZ_YEAR} Gazetteer"
        return 1
    fi
    if [[ $count -gt 1 ]]; then
        warn "\"$query\": ambiguous, $count matches -- re-run with one of these GEOIDs:"
        printf '%s\n' "$matches" | awk -F'\t' '{printf "         %s  %s, %s\n",$1,$2,$3}' >&2
        return 1
    fi
    printf '%s\n' "$matches"
}

# --------------------------------------------------------------------------
# Step 2: resolve every requested city up front, so a typo fails fast
# instead of halfway through a multi-gigabyte download.
# --------------------------------------------------------------------------
declare -a R_GEOID R_NAME R_USPS R_FIPS R_SLUG
FAILED=0
for q in "${CITY_ARGS[@]}"; do
    if ! row=$(resolve_city "$q"); then FAILED=$((FAILED+1)); continue; fi
    geoid=$(cut -f1 <<<"$row"); nlsad=$(cut -f2 <<<"$row"); usps=$(cut -f3 <<<"$row")
    fips=${geoid:0:2}
    if [[ -z $(lookup_state "$fips" 1 3) ]]; then
        warn "\"$q\": state FIPS $fips has no Geofabrik region; skipping"
        FAILED=$((FAILED+1)); continue
    fi
    R_GEOID+=("$geoid"); R_NAME+=("$nlsad"); R_USPS+=("$usps"); R_FIPS+=("$fips")
    R_SLUG+=("$(slugify "$nlsad")-$(printf '%s' "$usps" | tr '[:upper:]' '[:lower:]')-$geoid")
done

[[ ${#R_GEOID[@]} -gt 0 ]] || die "no cities resolved; nothing to do"

log "resolved ${#R_GEOID[@]} city/cities$( [[ $FAILED -gt 0 ]] && printf ' (%s unresolved)' "$FAILED" )"
for i in "${!R_GEOID[@]}"; do
    printf '         %s  %-28s %s\n' "${R_GEOID[$i]}" "${R_NAME[$i]}, ${R_USPS[$i]}" "${R_SLUG[$i]}" >&2
done

if [[ $DRY_RUN -eq 1 ]]; then
    log "dry run -- stopping before downloads"
    exit 0
fi

# --------------------------------------------------------------------------
# Step 3: per-state bulk sources (downloaded once, shared by all its cities)
# --------------------------------------------------------------------------
ensure_state_data() {
    local fips=$1 usps=$2 lc gf

    lc=$(printf '%s' "$usps" | tr '[:upper:]' '[:lower:]')
    gf=$(lookup_state "$fips" 1 3)

    # 3a. TIGER/Line PLACE -- city boundaries, already ESRI .shp + .dbf
    fetch_zip "$CENSUS/geo/tiger/TIGER${TIGER_YEAR}/PLACE/tl_${TIGER_YEAR}_${fips}_place.zip" \
              "$CACHE/zips/tl_${TIGER_YEAR}_${fips}_place.zip" \
              "$CACHE/tiger" "tl_${TIGER_YEAR}_${fips}_place.shp"

    if [[ $SKIP_PUMS -eq 0 ]]; then
        # 3b. TIGER/Line PUMA -- note the directory is PUMA20, not PUMA
        fetch_zip "$CENSUS/geo/tiger/TIGER${TIGER_YEAR}/PUMA20/tl_${TIGER_YEAR}_${fips}_puma20.zip" \
                  "$CACHE/zips/tl_${TIGER_YEAR}_${fips}_puma20.zip" \
                  "$CACHE/tiger" "tl_${TIGER_YEAR}_${fips}_puma20.shp"

        # 3c. ACS PUMS person + housing records
        local pums_base="$CENSUS/programs-surveys/acs/data/pums/${PUMS_YEAR}/${PUMS_SPAN}"
        fetch_zip "$pums_base/csv_p${lc}.zip" "$CACHE/zips/csv_p${lc}.zip" \
                  "$CACHE/pums/p${lc}" "psam_p${fips}.csv"
        fetch_zip "$pums_base/csv_h${lc}.zip" "$CACHE/zips/csv_h${lc}.zip" \
                  "$CACHE/pums/h${lc}" "psam_h${fips}.csv"
    fi

    if [[ $SKIP_OSM -eq 0 ]]; then
        # 3d. Geofabrik state extract. This is the big one (0.3-1.3 GB), but
        #     it is fetched once per state and then clipped locally, which is
        #     far more reliable than hitting Overpass once per city.
        fetch "$GEOFABRIK/${gf}-latest.osm.pbf" "$CACHE/osm/${gf}-latest.osm.pbf"
    fi
}

# --------------------------------------------------------------------------
# Step 4: per-city derivation
# --------------------------------------------------------------------------
build_city() {
    local geoid=$1 nlsad=$2 usps=$3 fips=$4 slug=$5
    local lc gf dir place_shp puma_shp pbf
    local PUMA_DBF_COL="" PUMA_PUMS_COL=""
    lc=$(printf '%s' "$usps" | tr '[:upper:]' '[:lower:]')
    gf=$(lookup_state "$fips" 1 3)
    dir="$CITYDIR/$slug"
    mkdir -p "$dir"

    place_shp="$CACHE/tiger/tl_${TIGER_YEAR}_${fips}_place.shp"

    # 4a. Clip the single place out of the statewide PLACE layer.
    log "boundary  $nlsad, $usps ($geoid)"
    rm -f "$dir"/boundary.shp "$dir"/boundary.dbf "$dir"/boundary.shx \
          "$dir"/boundary.prj "$dir"/boundary.cpg "$dir"/boundary.geojson
    ogr2ogr -f "ESRI Shapefile" "$dir/boundary.shp" "$place_shp" \
            -where "GEOID='$geoid'" || die "ogr2ogr failed for $geoid"
    ogr2ogr -f GeoJSON "$dir/boundary.geojson" "$place_shp" \
            -where "GEOID='$geoid'" || die "ogr2ogr (geojson) failed for $geoid"

    if ! grep -q '"type"' "$dir/boundary.geojson" 2>/dev/null; then
        die "empty boundary for GEOID $geoid -- wrong TIGER vintage?"
    fi

    # 4b. Clip the city's OSM data out of the state PBF, then emit OSM XML.
    if [[ $SKIP_OSM -eq 0 ]]; then
        pbf="$CACHE/osm/${gf}-latest.osm.pbf"
        log "osm       clipping $gf -> $slug"
        osmium extract --overwrite -p "$dir/boundary.geojson" "$pbf" \
               -o "$dir/city.osm.pbf" || die "osmium extract failed for $slug"
        osmium cat --overwrite "$dir/city.osm.pbf" -o "$dir/city.osm" \
               || die "osmium cat failed for $slug"
    fi

    # 4c. PUMA geometry + PUMS microdata for this state.
    if [[ $SKIP_PUMS -eq 0 ]]; then
        puma_shp="$CACHE/tiger/tl_${TIGER_YEAR}_${fips}_puma20.shp"
        log "puma/pums $usps"
        rm -f "$dir"/puma.shp "$dir"/puma.dbf "$dir"/puma.shx "$dir"/puma.prj "$dir"/puma.cpg
        ogr2ogr -f "ESRI Shapefile" "$dir/puma.shp" "$puma_shp" || die "ogr2ogr failed for PUMA $fips"
        cp -f "$CACHE/pums/p${lc}/psam_p${fips}.csv" "$dir/pums_p.csv"
        cp -f "$CACHE/pums/h${lc}/psam_h${fips}.csv" "$dir/pums_h.csv"

        # The PUMA id column differs between the two sources: PUMS CSVs call it
        # "PUMA", while the TIGER shapefile's DBF calls it "PUMACE20" (or
        # "PUMACE10" for the 2010 vintage). modelgen defaults BOTH to "PUMA",
        # so the join silently finds nothing unless we name them explicitly.
        PUMA_DBF_COL=$(ogrinfo -so "$dir/puma.shp" puma 2>/dev/null \
                       | awk -F':' '/^PUMACE[0-9]*:/ { print $1; exit }')
        if [[ -z $PUMA_DBF_COL ]]; then
            PUMA_DBF_COL=$(ogrinfo -so "$dir/puma.shp" puma 2>/dev/null \
                           | awk -F':' '/^[A-Z0-9_]*PUMA[A-Z0-9_]*:/ { print $1; exit }')
        fi
        [[ -n $PUMA_DBF_COL ]] || warn "could not detect a PUMA id column in $dir/puma.shp"

        PUMA_PUMS_COL=$(head -1 "$dir/pums_p.csv" | tr ',' '\n' | tr -d '"\r' \
                        | awk '$0=="PUMA" { print; exit }')
        [[ -n $PUMA_PUMS_COL ]] || PUMA_PUMS_COL=PUMA
    fi

    # 4d. Ready-to-use modelgen arguments.
    {
        printf '# modelgen inputs for %s, %s (GEOID %s)\n' "$nlsad" "$usps" "$geoid"
        printf '# generated %s by %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$(basename "$0")"
        printf '# NOTE: --pop-gis is not produced by this script.\n'
        printf -- '--shape %s/boundary.shp\n' "$dir"
        printf -- '--dbf %s/boundary.dbf\n' "$dir"
        [[ $SKIP_OSM  -eq 0 ]] && printf -- '--osm-xml %s/city.osm\n' "$dir"
        if [[ $SKIP_PUMS -eq 0 ]]; then
            printf -- '--puma-shp %s/puma.shp\n' "$dir"
            printf -- '--puma-dbf %s/puma.dbf\n' "$dir"
            printf -- '--pums-p %s/pums_p.csv\n' "$dir"
            printf -- '--pums-h %s/pums_h.csv\n' "$dir"
            [[ -n ${PUMA_DBF_COL:-}  ]] && printf -- '--puma-id-col-dbf %s\n'  "$PUMA_DBF_COL"
            [[ -n ${PUMA_PUMS_COL:-} ]] && printf -- '--puma-id-col-pums %s\n' "$PUMA_PUMS_COL"
        fi
    } > "$dir/modelgen.args"
}

# --------------------------------------------------------------------------
# Drive: group by state so each bulk source is fetched exactly once.
# --------------------------------------------------------------------------
STATES_DONE=""
for i in "${!R_GEOID[@]}"; do
    fips=${R_FIPS[$i]}
    if [[ " $STATES_DONE " != *" $fips "* ]]; then
        log "=== state ${R_USPS[$i]} (FIPS $fips) ==="
        ensure_state_data "$fips" "${R_USPS[$i]}"
        STATES_DONE="$STATES_DONE $fips"
    fi
done

for i in "${!R_GEOID[@]}"; do
    log "=== ${R_NAME[$i]}, ${R_USPS[$i]} ==="
    build_city "${R_GEOID[$i]}" "${R_NAME[$i]}" "${R_USPS[$i]}" "${R_FIPS[$i]}" "${R_SLUG[$i]}"
done

log "done -- ${#R_GEOID[@]} city/cities under $CITYDIR"
[[ $FAILED -gt 0 ]] && warn "$FAILED input(s) could not be resolved (see above)"
exit 0
