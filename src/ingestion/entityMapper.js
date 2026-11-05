/**
 * Entity Mapper
 * Transforms raw Wikidata SPARQL binding rows into normalized COSMO entities
 * with aliases, structured attributes, and quality completeness scores.
 */

class EntityMapper {
    /**
     * Map a raw SPARQL result binding to a normalized entity object.
     * @param {Object} binding - Single SPARQL binding item
     * @param {Object} categoryDef - Category definition metadata
     */
    static mapBinding(binding, categoryDef) {
        if (!binding || !binding.item || !binding.item.value) {
            return null;
        }

        // Extract QID from URI (e.g. http://www.wikidata.org/entity/Q20150 -> Q20150)
        const uri = binding.item.value;
        const qidMatch = uri.match(/Q[0-9]+$/);
        if (!qidMatch) return null;
        const wikidataId = qidMatch[0];

        // Primary name
        const rawName = binding.itemLabel ? binding.itemLabel.value.trim() : '';
        if (!rawName || /^Q[0-9]+$/.test(rawName)) {
            // No valid English label (Wikidata returned QID placeholder)
            return null;
        }

        // Description
        const description = binding.itemDescription && !/^Q[0-9]+$/.test(binding.itemDescription.value)
            ? binding.itemDescription.value.trim()
            : null;

        // Image URL
        let imageUrl = null;
        if (binding.image && binding.image.value) {
            imageUrl = binding.image.value;
            // Ensure HTTPS
            if (imageUrl.startsWith('http://')) {
                imageUrl = 'https://' + imageUrl.slice(7);
            }
        }

        // Sitelinks & Popularity Rank
        const sitelinks = binding.sitelinks ? parseInt(binding.sitelinks.value, 10) : 0;
        const popularityRank = Math.max(1, 1000000 - (sitelinks * 1000));

        // Gender
        let gender = null;
        if (binding.genderLabel && binding.genderLabel.value) {
            const g = binding.genderLabel.value.toLowerCase();
            if (g.includes('female') || g === 'woman') gender = 'female';
            else if (g.includes('male') || g === 'man') gender = 'male';
            else if (g.includes('non-binary')) gender = 'non-binary';
        }

        // Country / Nationality
        const nationality = binding.countryLabel && !/^Q[0-9]+$/.test(binding.countryLabel.value)
            ? binding.countryLabel.value.trim()
            : null;

        // Dates
        const birthDate = EntityMapper._cleanDate(binding.birth ? binding.birth.value : null);
        const deathDate = EntityMapper._cleanDate(binding.death ? binding.death.value : null);

        // Subcategory (group, series, or occupation)
        let subcategory = null;
        if (binding.groupLabel && !/^Q[0-9]+$/.test(binding.groupLabel.value)) {
            subcategory = binding.groupLabel.value.trim();
        } else if (binding.seriesLabel && !/^Q[0-9]+$/.test(binding.seriesLabel.value)) {
            subcategory = binding.seriesLabel.value.trim();
        } else if (binding.occupationLabel && !/^Q[0-9]+$/.test(binding.occupationLabel.value)) {
            subcategory = binding.occupationLabel.value.trim();
        }

        // Aliases
        const aliases = EntityMapper._extractAliases(binding.itemAltLabel ? binding.itemAltLabel.value : null, rawName);

        // Attributes extraction
        const isFictional = categoryDef.isFictional !== undefined ? categoryDef.isFictional : 0;
        const attributes = EntityMapper._extractAttributes({
            rawName,
            description,
            category: categoryDef.categoryName,
            isFictional,
            gender,
            nationality,
            birthDate,
            deathDate,
            occupation: binding.occupationLabel ? binding.occupationLabel.value : null,
            group: binding.groupLabel ? binding.groupLabel.value : null,
            series: binding.seriesLabel ? binding.seriesLabel.value : null
        });

        // Calculate completeness score (0 to 100)
        const completenessScore = EntityMapper.calculateCompleteness({
            name: rawName,
            description,
            imageUrl,
            gender,
            nationality,
            birthDate,
            deathDate,
            aliases,
            attributes
        });

        return {
            wikidata_id: wikidataId,
            name: rawName,
            description,
            entity_type: isFictional ? 'fictional_character' : 'human',
            category: categoryDef.categoryName,
            subcategory,
            image_url: imageUrl,
            gender,
            nationality,
            birth_date: birthDate,
            death_date: deathDate,
            debut_year: birthDate ? parseInt(birthDate.slice(0, 4), 10) : null,
            is_fictional: isFictional,
            popularity_rank: popularityRank,
            source: 'wikidata',
            aliases,
            attributes,
            completenessScore,
            is_game_ready: completenessScore >= 40
        };
    }

    /**
     * Parse date string into YYYY-MM-DD or YYYY format.
     */
    static _cleanDate(dateStr) {
        if (!dateStr || typeof dateStr !== 'string') return null;
        // Matches e.g. "1997-09-01T00:00:00Z" -> "1997-09-01"
        const match = dateStr.match(/^([+-]?\d{4})-(\d{2})-(\d{2})/);
        if (match) {
            return `${match[1]}-${match[2]}-${match[3]}`;
        }
        const yearMatch = dateStr.match(/^([+-]?\d{4})/);
        return yearMatch ? yearMatch[1] : null;
    }

    /**
     * Extract and deduplicate aliases from Wikidata's comma-separated alt labels.
     */
    static _extractAliases(altLabelStr, primaryName) {
        if (!altLabelStr || typeof altLabelStr !== 'string') return [];
        const seen = new Set();
        const primaryLower = primaryName.toLowerCase().trim();

        return altLabelStr
            .split(',')
            .map(s => s.trim())
            .filter(s => {
                if (!s || s.length < 2 || s.length > 80) return false;
                if (/^Q[0-9]+$/.test(s)) return false;
                const lower = s.toLowerCase();
                if (lower === primaryLower) return false;
                if (seen.has(lower)) return false;
                seen.add(lower);
                return true;
            });
    }

    /**
     * Map verified properties to boolean/categorical engine attributes.
     */
    static _extractAttributes(ctx) {
        const attrs = {};
        const {
            rawName,
            description = '',
            category,
            isFictional,
            gender,
            nationality,
            birthDate,
            deathDate,
            occupation = '',
            group,
            series = ''
        } = ctx;

        const fullText = `${rawName} ${description || ''} ${occupation || ''} ${series || ''}`.toLowerCase();

        // 1. Real vs Fictional
        attrs.is_real = isFictional ? '0' : '1';

        // 2. Gender
        if (gender === 'male') {
            attrs.is_male = '1';
            attrs.is_female = '0';
        } else if (gender === 'female') {
            attrs.is_male = '0';
            attrs.is_female = '1';
        }

        // 3. Category-specific flags
        if (category === 'K-pop') {
            attrs.is_kpop = '1';
            attrs.is_singer = '1';
            attrs.from_south_korea = '1';
        }
        if (category === 'Music') {
            attrs.is_singer = '1';
        }
        if (category === 'Hollywood' || category === 'Bollywood') {
            attrs.is_actor = '1';
        }
        if (category === 'Sports') {
            attrs.is_athlete = '1';
        }
        if (category === 'Anime & Manga') {
            attrs.is_anime = '1';
            attrs.from_japan = '1';
            attrs.has_superpowers = '1';
        }
        if (category === 'Cartoons') {
            attrs.is_cartoon = '1';
        }
        if (category === 'Video Games') {
            attrs.is_video_game = '1';
        }
        if (category === 'Superheroes & Villains') {
            attrs.has_superpowers = '1';
            if (fullText.includes('villain') || fullText.includes('antagonist')) {
                attrs.is_villain = '1';
                attrs.is_superhero = '0';
            } else {
                attrs.is_superhero = '1';
                attrs.is_villain = '0';
            }
        }

        // 4. Franchise associations
        if (fullText.includes('marvel') || fullText.includes('avengers') || fullText.includes('spider-man') || fullText.includes('x-men')) {
            attrs.marvel_universe = '1';
            attrs.dc_universe = '0';
        } else if (fullText.includes('dc comics') || fullText.includes('batman') || fullText.includes('superman') || fullText.includes('justice league')) {
            attrs.dc_universe = '1';
            attrs.marvel_universe = '0';
        }

        // 5. Nationality
        if (nationality) {
            const nat = nationality.toLowerCase();
            if (nat.includes('south korea') || nat.includes('korea')) attrs.from_south_korea = '1';
            if (nat.includes('united states') || nat.includes('american')) attrs.from_usa = '1';
            if (nat.includes('japan')) attrs.from_japan = '1';
            if (nat.includes('india')) attrs.from_india = '1';
            if (nat.includes('united kingdom') || nat.includes('british') || nat.includes('england')) attrs.from_uk = '1';
        }

        // 6. Alive / Historical
        if (deathDate) {
            attrs.is_alive = '0';
            const deathYear = parseInt(deathDate.slice(0, 4), 10);
            if (!isNaN(deathYear) && deathYear < 2000) {
                attrs.is_historical = '1';
            }
        } else if (!isFictional && birthDate) {
            const birthYear = parseInt(birthDate.slice(0, 4), 10);
            if (!isNaN(birthYear) && birthYear >= 1925) {
                attrs.is_alive = '1';
            }
        }

        // 7. Group member
        if (group || category === 'K-pop' && fullText.includes('member of')) {
            attrs.is_group_member = '1';
        }

        // 8. YouTuber / Creator
        if (fullText.includes('youtuber') || fullText.includes('youtube personality') || fullText.includes('content creator')) {
            attrs.is_youtuber = '1';
        }

        // 9. Football / Soccer
        if (fullText.includes('footballer') || fullText.includes('association football') || fullText.includes('soccer player')) {
            attrs.plays_football = '1';
        }

        return attrs;
    }

    /**
     * Compute quality completeness percentage (0-100).
     */
    static calculateCompleteness(entity) {
        let score = 0;
        if (entity.name) score += 10;
        if (entity.description) score += 15;
        if (entity.imageUrl) score += 15;
        if (entity.gender) score += 15;
        if (entity.nationality) score += 15;
        if (entity.birthDate || entity.deathDate) score += 15;
        if (entity.aliases && entity.aliases.length > 0) score += 10;
        if (entity.attributes && Object.keys(entity.attributes).length >= 4) score += 5;

        return Math.min(100, score);
    }
}

module.exports = EntityMapper;

