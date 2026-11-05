/**
 * SPARQL Query Builder for Wikidata Ingestion
 * Generates targeted, paginated SPARQL queries for all 14 COSMO categories.
 */

const CATEGORY_DEFINITIONS = {
    kpop: {
        categoryName: 'K-pop',
        isFictional: 0,
        pattern: `
            ?item wdt:P31 ?type.
            VALUES ?type { wd:Q5 wd:Q215380 wd:Q2088357 }
            { ?item wdt:P136 wd:Q213941. }
            UNION
            { ?item wdt:P27 wd:Q884; wdt:P106 ?kpopOcc. VALUES ?kpopOcc { wd:Q177220 wd:Q639669 wd:Q5716684 } }
        `
    },
    international_musicians: {
        categoryName: 'Music',
        isFictional: 0,
        pattern: `
            ?item wdt:P31 wd:Q5;
                  wdt:P106 ?occ.
            VALUES ?occ { wd:Q177220 wd:Q639669 wd:Q753110 }
        `
    },
    bollywood_actors: {
        categoryName: 'Bollywood',
        isFictional: 0,
        pattern: `
            ?item wdt:P31 wd:Q5;
                  wdt:P106 wd:Q33999;
                  wdt:P27 wd:Q668.
        `
    },
    hollywood_actors: {
        categoryName: 'Hollywood',
        isFictional: 0,
        pattern: `
            ?item wdt:P31 wd:Q5;
                  wdt:P106 wd:Q33999;
                  wdt:P27 wd:Q30.
        `
    },
    celebrities: {
        categoryName: 'Celebrity',
        isFictional: 0,
        pattern: `
            ?item wdt:P31 wd:Q5;
                  wdt:P106 ?celebOcc.
            VALUES ?celebOcc { wd:Q2066131 wd:Q61448839 wd:Q11514315 wd:Q10800557 }
        `
    },
    athletes: {
        categoryName: 'Sports',
        isFictional: 0,
        pattern: `
            ?item wdt:P31 wd:Q5;
                  wdt:P106 ?sportOcc.
            VALUES ?sportOcc { wd:Q937857 wd:Q10833314 wd:Q1151308 wd:Q10871364 wd:Q2066131 }
        `
    },
    anime_manga: {
        categoryName: 'Anime & Manga',
        isFictional: 1,
        pattern: `
            ?item wdt:P31 ?animeCharType.
            VALUES ?animeCharType { wd:Q95074 wd:Q15632617 wd:Q1114444 }
            ?item wdt:P495 wd:Q17.
        `
    },
    movie_characters: {
        categoryName: 'Movies & TV',
        isFictional: 1,
        pattern: `
            ?item wdt:P31 wd:Q95074;
                  wdt:P1441 ?filmWork.
            ?filmWork wdt:P31/wdt:P279* wd:Q11424.
        `
    },
    tv_characters: {
        categoryName: 'Movies & TV',
        isFictional: 1,
        pattern: `
            ?item wdt:P31 wd:Q95074;
                  wdt:P1441 ?tvWork.
            ?tvWork wdt:P31/wdt:P279* wd:Q5398426.
        `
    },
    cartoon_characters: {
        categoryName: 'Cartoons',
        isFictional: 1,
        pattern: `
            ?item wdt:P31 ?cartoonType.
            VALUES ?cartoonType { wd:Q95074 wd:Q15705348 }
            OPTIONAL { ?item wdt:P495 ?country. }
            FILTER(!BOUND(?country) || ?country != wd:Q17)
        `
    },
    video_game_characters: {
        categoryName: 'Video Games',
        isFictional: 1,
        pattern: `
            ?item wdt:P31 ?gameCharType.
            VALUES ?gameCharType { wd:Q95074 wd:Q226730 }
            { ?item wdt:P1441 ?gameWork. ?gameWork wdt:P31/wdt:P279* wd:Q7889. }
            UNION
            { ?item wdt:P31 wd:Q226730. }
        `
    },
    superheroes_villains: {
        categoryName: 'Superheroes & Villains',
        isFictional: 1,
        pattern: `
            ?item wdt:P31 ?heroType.
            VALUES ?heroType { wd:Q188784 wd:Q1058913 wd:Q10834383 }
        `
    },
    historical_figures: {
        categoryName: 'Historical & Notable',
        isFictional: 0,
        pattern: `
            ?item wdt:P31 wd:Q5;
                  wdt:P570 ?death.
            FILTER(?death < "2000-01-01"^^xsd:dateTime)
        `
    },
    notable_entities: {
        categoryName: 'Historical & Notable',
        isFictional: 0,
        pattern: `
            ?item wdt:P31 wd:Q5;
                  wdt:P106 ?notableOcc.
            VALUES ?notableOcc { wd:Q901 wd:Q169470 wd:Q4964182 wd:Q482980 }
        `
    }
};

class QueryBuilder {
    /**
     * Get list of supported category keys.
     */
    static getCategories() {
        return Object.keys(CATEGORY_DEFINITIONS);
    }

    /**
     * Get metadata for a specific category key.
     */
    static getCategoryDefinition(categoryKey) {
        return CATEGORY_DEFINITIONS[categoryKey];
    }

    /**
     * Build an optimized, paginated SPARQL query for the given category.
     * @param {string} categoryKey - One of the supported category keys
     * @param {Object} options - Pagination and filtering options
     * @param {number} options.limit - Number of entities to fetch (default: 50)
     * @param {number} options.offset - Pagination offset (default: 0)
     * @param {number} options.minSitelinks - Minimum Wikipedia sitelinks (default: 2)
     */
    static buildQuery(categoryKey, options = {}) {
        const def = CATEGORY_DEFINITIONS[categoryKey];
        if (!def) {
            throw new Error(`Unsupported category key: '${categoryKey}'. Supported: ${Object.keys(CATEGORY_DEFINITIONS).join(', ')}`);
        }

        const limit = Math.max(1, parseInt(options.limit, 10) || 50);
        const offset = Math.max(0, parseInt(options.offset, 10) || 0);
        const minSitelinks = options.minSitelinks !== undefined ? options.minSitelinks : 2;

        return `
SELECT DISTINCT 
  ?item 
  ?itemLabel 
  ?itemDescription 
  ?itemAltLabel 
  ?sitelinks
  ?genderLabel 
  ?countryLabel 
  ?birth 
  ?death 
  ?image 
  ?occupationLabel 
  ?groupLabel 
  ?seriesLabel
WHERE {
  ${def.pattern}
  
  ?item wikibase:sitelinks ?sitelinks.
  FILTER(?sitelinks >= ${minSitelinks})

  OPTIONAL { ?item wdt:P21 ?gender. }
  OPTIONAL { ?item wdt:P27 ?country. }
  OPTIONAL { ?item wdt:P569 ?birth. }
  OPTIONAL { ?item wdt:P570 ?death. }
  OPTIONAL { ?item wdt:P18 ?image. }
  OPTIONAL { ?item wdt:P106 ?occupation. }
  OPTIONAL { ?item wdt:P463 ?group. }
  OPTIONAL { ?item wdt:P179 ?series. }

  SERVICE wikibase:label { 
    bd:serviceParam wikibase:language "en". 
  }
}
ORDER BY DESC(?sitelinks)
LIMIT ${limit}
OFFSET ${offset}
        `.trim();
    }
}

module.exports = {
    QueryBuilder,
    CATEGORY_DEFINITIONS
};

