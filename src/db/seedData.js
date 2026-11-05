// Comprehensive seed dataset covering all 11 required categories
// with authentic Wikidata IDs, attributes, aliases, and metadata.

const categories = [
    { name: 'K-pop', description: 'K-pop idols, artists, and groups' },
    { name: 'Music', description: 'International singers, musicians, and bands' },
    { name: 'Hollywood', description: 'Hollywood actors, filmmakers, and cinema icons' },
    { name: 'Bollywood', description: 'Bollywood actors, playback singers, and cinema icons' },
    { name: 'Celebrity', description: 'Celebrities, influencers, and internet personalities' },
    { name: 'Sports', description: 'Athletes and sports icons' },
    { name: 'Anime & Manga', description: 'Characters from anime, manga, and light novels' },
    { name: 'Movies & TV', description: 'Characters from live-action movies and television shows' },
    { name: 'Cartoons', description: 'Characters from animated television and classic cartoons' },
    { name: 'Video Games', description: 'Protagonists and icons from video games' },
    { name: 'Superheroes & Villains', description: 'Comic book and cinematic superheroes and villains' },
    { name: 'Historical & Notable', description: 'Historical figures, scientists, and world leaders' }
];

const attributes = [
    {
        attribute_key: 'is_real',
        attribute_name: 'Real Person',
        data_type: 'boolean',
        question_template: 'Is your character a real person?'
    },
    {
        attribute_key: 'is_male',
        attribute_name: 'Male',
        data_type: 'boolean',
        question_template: 'Is your character male?'
    },
    {
        attribute_key: 'is_female',
        attribute_name: 'Female',
        data_type: 'boolean',
        question_template: 'Is your character female?'
    },
    {
        attribute_key: 'is_singer',
        attribute_name: 'Singer / Musician',
        data_type: 'boolean',
        question_template: 'Is your character a singer or musical artist?'
    },
    {
        attribute_key: 'is_actor',
        attribute_name: 'Actor / Actress',
        data_type: 'boolean',
        question_template: 'Is your character an actor or actress?'
    },
    {
        attribute_key: 'is_athlete',
        attribute_name: 'Athlete / Sports Personality',
        data_type: 'boolean',
        question_template: 'Is your character an athlete or sports star?'
    },
    {
        attribute_key: 'is_kpop',
        attribute_name: 'K-pop Association',
        data_type: 'boolean',
        question_template: 'Is your character associated with K-pop?'
    },
    {
        attribute_key: 'is_anime',
        attribute_name: 'Anime / Manga',
        data_type: 'boolean',
        question_template: 'Is your character from an anime or manga?'
    },
    {
        attribute_key: 'is_superhero',
        attribute_name: 'Superhero',
        data_type: 'boolean',
        question_template: 'Is your character a superhero?'
    },
    {
        attribute_key: 'is_villain',
        attribute_name: 'Villain',
        data_type: 'boolean',
        question_template: 'Is your character a villain or antagonist?'
    },
    {
        attribute_key: 'is_video_game',
        attribute_name: 'Video Game Origin',
        data_type: 'boolean',
        question_template: 'Does your character originate from a video game?'
    },
    {
        attribute_key: 'is_cartoon',
        attribute_name: 'Cartoon Character',
        data_type: 'boolean',
        question_template: 'Is your character an animated cartoon character?'
    },
    {
        attribute_key: 'has_superpowers',
        attribute_name: 'Supernatural Abilities',
        data_type: 'boolean',
        question_template: 'Does your character possess supernatural abilities or superpowers?'
    },
    {
        attribute_key: 'marvel_universe',
        attribute_name: 'Marvel Universe',
        data_type: 'boolean',
        question_template: 'Is your character part of the Marvel universe?'
    },
    {
        attribute_key: 'dc_universe',
        attribute_name: 'DC Universe',
        data_type: 'boolean',
        question_template: 'Is your character part of the DC universe?'
    },
    {
        attribute_key: 'from_south_korea',
        attribute_name: 'From South Korea',
        data_type: 'boolean',
        question_template: 'Is your character from South Korea?'
    },
    {
        attribute_key: 'from_usa',
        attribute_name: 'From United States',
        data_type: 'boolean',
        question_template: 'Is your character from the United States?'
    },
    {
        attribute_key: 'from_japan',
        attribute_name: 'From Japan',
        data_type: 'boolean',
        question_template: 'Is your character from Japan?'
    },
    {
        attribute_key: 'from_india',
        attribute_name: 'From India',
        data_type: 'boolean',
        question_template: 'Is your character from India?'
    },
    {
        attribute_key: 'from_uk',
        attribute_name: 'From United Kingdom',
        data_type: 'boolean',
        question_template: 'Is your character from the United Kingdom?'
    },
    {
        attribute_key: 'debut_after_2015',
        attribute_name: 'Debuted After 2015',
        data_type: 'boolean',
        question_template: 'Did your character debut or achieve fame after 2015?'
    },
    {
        attribute_key: 'is_alive',
        attribute_name: 'Currently Alive',
        data_type: 'boolean',
        question_template: 'Is your character currently alive?'
    },
    {
        attribute_key: 'is_historical',
        attribute_name: 'Historical Figure',
        data_type: 'boolean',
        question_template: 'Is your character a historical figure from before the 21st century?'
    },
    {
        attribute_key: 'is_group_member',
        attribute_name: 'Member of a Group / Band',
        data_type: 'boolean',
        question_template: 'Is your character famously part of a group or band?'
    },
    {
        attribute_key: 'is_youtuber',
        attribute_name: 'Content Creator / YouTuber',
        data_type: 'boolean',
        question_template: 'Is your character a YouTuber or internet creator?'
    },
    {
        attribute_key: 'plays_football',
        attribute_name: 'Football / Soccer Player',
        data_type: 'boolean',
        question_template: 'Is your character famous for playing football (soccer)?'
    },
    {
        attribute_key: 'uses_magic',
        attribute_name: 'Uses Magic',
        data_type: 'boolean',
        question_template: 'Does your character use magic, spells, or wizardry?'
    },
    {
        attribute_key: 'wears_mask',
        attribute_name: 'Wears Mask / Costume',
        data_type: 'boolean',
        question_template: 'Does your character wear a mask or helmet to conceal their identity?'
    },
    {
        attribute_key: 'is_scientist',
        attribute_name: 'Scientist / Inventor',
        data_type: 'boolean',
        question_template: 'Is your character a scientist, physicist, or inventor?'
    }
];

const entities = [
    // 1. K-pop
    {
        wikidata_id: 'Q20823729',
        name: 'Jungkook',
        description: 'South Korean singer, member of BTS',
        entity_type: 'human',
        category: 'K-pop',
        subcategory: 'BTS',
        image_url: 'https://upload.wikimedia.org/wikipedia/commons/thumb/e/e0/Jeon_Jung-kook_at_the_White_House%2C_31_May_2022.jpg/440px-Jeon_Jung-kook_at_the_White_House%2C_31_May_2022.jpg',
        gender: 'male',
        nationality: 'South Korea',
        birth_date: '1997-09-01',
        debut_year: 2013,
        is_fictional: 0,
        popularity_rank: 10,
        aliases: ['Jeon Jung-kook', 'JK', 'Golden Maknae'],
        attributes: {
            is_real: '1',
            is_male: '1',
            is_female: '0',
            is_singer: '1',
            is_actor: '0',
            is_athlete: '0',
            is_kpop: '1',
            from_south_korea: '1',
            from_usa: '0',
            is_group_member: '1',
            is_alive: '1',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q26262600',
        name: 'Jennie',
        description: 'South Korean singer and rapper, member of Blackpink',
        entity_type: 'human',
        category: 'K-pop',
        subcategory: 'BLACKPINK',
        image_url: 'https://upload.wikimedia.org/wikipedia/commons/thumb/1/14/Jennie_Kim_for_Chanel_in_March_2024.jpg/440px-Jennie_Kim_for_Chanel_in_March_2024.jpg',
        gender: 'female',
        nationality: 'South Korea',
        birth_date: '1996-01-16',
        debut_year: 2016,
        is_fictional: 0,
        popularity_rank: 15,
        aliases: ['Jennie Kim', 'Kim Jennie', 'Ruby Jane'],
        attributes: {
            is_real: '1',
            is_male: '0',
            is_female: '1',
            is_singer: '1',
            is_actor: '1',
            is_athlete: '0',
            is_kpop: '1',
            from_south_korea: '1',
            from_usa: '0',
            is_group_member: '1',
            is_alive: '1',
            debut_after_2015: '1'
        }
    },
    {
        wikidata_id: 'Q20145',
        name: 'IU',
        description: 'South Korean singer-songwriter and actress',
        entity_type: 'human',
        category: 'K-pop',
        subcategory: 'Soloist',
        image_url: 'https://upload.wikimedia.org/wikipedia/commons/thumb/c/c5/IU_at_the_Golden_Disc_Awards_2022.jpg/440px-IU_at_the_Golden_Disc_Awards_2022.jpg',
        gender: 'female',
        nationality: 'South Korea',
        birth_date: '1993-05-16',
        debut_year: 2008,
        is_fictional: 0,
        popularity_rank: 20,
        aliases: ['Lee Ji-eun', 'Nation\'s Little Sister'],
        attributes: {
            is_real: '1',
            is_male: '0',
            is_female: '1',
            is_singer: '1',
            is_actor: '1',
            is_kpop: '1',
            from_south_korea: '1',
            is_group_member: '0',
            is_alive: '1',
            debut_after_2015: '0'
        }
    },

    // 2. Music
    {
        wikidata_id: 'Q26876',
        name: 'Taylor Swift',
        description: 'American singer-songwriter',
        entity_type: 'human',
        category: 'Music',
        subcategory: 'Pop / Country',
        gender: 'female',
        nationality: 'United States',
        birth_date: '1989-12-13',
        debut_year: 2006,
        is_fictional: 0,
        popularity_rank: 1,
        aliases: ['Taylor Alison Swift', 'T-Swizzle'],
        attributes: {
            is_real: '1',
            is_male: '0',
            is_female: '1',
            is_singer: '1',
            is_actor: '0',
            is_kpop: '0',
            from_usa: '1',
            is_group_member: '0',
            is_alive: '1',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q15869',
        name: 'Freddie Mercury',
        description: 'British musician and lead vocalist of Queen',
        entity_type: 'human',
        category: 'Music',
        subcategory: 'Rock',
        gender: 'male',
        nationality: 'United Kingdom',
        birth_date: '1946-09-05',
        death_date: '1991-11-24',
        debut_year: 1969,
        is_fictional: 0,
        popularity_rank: 12,
        aliases: ['Farrokh Bulsara', 'Queen front-man'],
        attributes: {
            is_real: '1',
            is_male: '1',
            is_female: '0',
            is_singer: '1',
            from_uk: '1',
            is_group_member: '1',
            is_alive: '0',
            is_historical: '1',
            debut_after_2015: '0'
        }
    },

    // 3. Hollywood
    {
        wikidata_id: 'Q38111',
        name: 'Leonardo DiCaprio',
        description: 'American actor and film producer',
        entity_type: 'human',
        category: 'Hollywood',
        gender: 'male',
        nationality: 'United States',
        birth_date: '1974-11-11',
        debut_year: 1989,
        is_fictional: 0,
        popularity_rank: 5,
        aliases: ['Leo DiCaprio', 'Leonardo Wilhelm DiCaprio'],
        attributes: {
            is_real: '1',
            is_male: '1',
            is_female: '0',
            is_actor: '1',
            is_singer: '0',
            from_usa: '1',
            is_alive: '1',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q34436',
        name: 'Scarlett Johansson',
        description: 'American actress',
        entity_type: 'human',
        category: 'Hollywood',
        gender: 'female',
        nationality: 'United States',
        birth_date: '1984-11-22',
        debut_year: 1994,
        is_fictional: 0,
        popularity_rank: 8,
        aliases: ['ScarJo'],
        attributes: {
            is_real: '1',
            is_male: '0',
            is_female: '1',
            is_actor: '1',
            is_singer: '0',
            from_usa: '1',
            is_alive: '1',
            debut_after_2015: '0'
        }
    },

    // 4. Bollywood
    {
        wikidata_id: 'Q9535',
        name: 'Shah Rukh Khan',
        description: 'Indian actor and film producer, known as the King of Bollywood',
        entity_type: 'human',
        category: 'Bollywood',
        gender: 'male',
        nationality: 'India',
        birth_date: '1965-11-02',
        debut_year: 1988,
        is_fictional: 0,
        popularity_rank: 4,
        aliases: ['SRK', 'King Khan', 'Badshah of Bollywood'],
        attributes: {
            is_real: '1',
            is_male: '1',
            is_female: '0',
            is_actor: '1',
            is_singer: '0',
            from_india: '1',
            is_alive: '1',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q159178',
        name: 'Deepika Padukone',
        description: 'Indian actress who works in Hindi films',
        entity_type: 'human',
        category: 'Bollywood',
        gender: 'female',
        nationality: 'India',
        birth_date: '1986-01-05',
        debut_year: 2006,
        is_fictional: 0,
        popularity_rank: 16,
        aliases: ['Deepika'],
        attributes: {
            is_real: '1',
            is_male: '0',
            is_female: '1',
            is_actor: '1',
            from_india: '1',
            is_alive: '1',
            debut_after_2015: '0'
        }
    },

    // 5. Celebrity & Influencer
    {
        wikidata_id: 'Q56256247',
        name: 'MrBeast',
        description: 'American YouTuber and philanthropist',
        entity_type: 'human',
        category: 'Celebrity',
        gender: 'male',
        nationality: 'United States',
        birth_date: '1998-05-07',
        debut_year: 2012,
        is_fictional: 0,
        popularity_rank: 2,
        aliases: ['Jimmy Donaldson', 'James Stephen Donaldson'],
        attributes: {
            is_real: '1',
            is_male: '1',
            is_female: '0',
            is_youtuber: '1',
            is_singer: '0',
            is_actor: '0',
            from_usa: '1',
            is_alive: '1',
            debut_after_2015: '0'
        }
    },

    // 6. Sports
    {
        wikidata_id: 'Q615',
        name: 'Lionel Messi',
        description: 'Argentine professional footballer',
        entity_type: 'human',
        category: 'Sports',
        subcategory: 'Football',
        gender: 'male',
        nationality: 'Argentina',
        birth_date: '1987-06-24',
        debut_year: 2004,
        is_fictional: 0,
        popularity_rank: 3,
        aliases: ['Leo Messi', 'La Pulga', 'Lionel Andrés Messi'],
        attributes: {
            is_real: '1',
            is_male: '1',
            is_female: '0',
            is_athlete: '1',
            plays_football: '1',
            is_alive: '1',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q213854',
        name: 'Virat Kohli',
        description: 'Indian international cricketer',
        entity_type: 'human',
        category: 'Sports',
        subcategory: 'Cricket',
        gender: 'male',
        nationality: 'India',
        birth_date: '1988-11-05',
        debut_year: 2008,
        is_fictional: 0,
        popularity_rank: 6,
        aliases: ['King Kohli', 'Cheeku'],
        attributes: {
            is_real: '1',
            is_male: '1',
            is_female: '0',
            is_athlete: '1',
            from_india: '1',
            plays_football: '0',
            is_alive: '1',
            debut_after_2015: '0'
        }
    },

    // 7. Anime & Manga
    {
        wikidata_id: 'Q2142',
        name: 'Goku',
        description: 'Protagonist of the Dragon Ball manga and anime franchise',
        entity_type: 'fictional_character',
        category: 'Anime & Manga',
        gender: 'male',
        nationality: 'Japan',
        debut_year: 1984,
        is_fictional: 1,
        popularity_rank: 7,
        aliases: ['Son Goku', 'Kakarot'],
        attributes: {
            is_real: '0',
            is_male: '1',
            is_female: '0',
            is_anime: '1',
            from_japan: '1',
            has_superpowers: '1',
            is_video_game: '0',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q172346',
        name: 'Naruto Uzumaki',
        description: 'Main protagonist of the manga and anime franchise Naruto',
        entity_type: 'fictional_character',
        category: 'Anime & Manga',
        gender: 'male',
        nationality: 'Japan',
        debut_year: 1999,
        is_fictional: 1,
        popularity_rank: 9,
        aliases: ['Naruto', 'Seventh Hokage'],
        attributes: {
            is_real: '0',
            is_male: '1',
            is_female: '0',
            is_anime: '1',
            from_japan: '1',
            has_superpowers: '1',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q186008',
        name: 'Monkey D. Luffy',
        description: 'Main protagonist of the One Piece anime and manga series',
        entity_type: 'fictional_character',
        category: 'Anime & Manga',
        gender: 'male',
        nationality: 'Japan',
        debut_year: 1997,
        is_fictional: 1,
        popularity_rank: 10,
        aliases: ['Straw Hat Luffy', 'Mugiwara', 'Luffy'],
        attributes: {
            is_real: '0',
            is_male: '1',
            is_female: '0',
            is_anime: '1',
            from_japan: '1',
            has_superpowers: '1',
            debut_after_2015: '0'
        }
    },

    // 8. Movies & TV
    {
        wikidata_id: 'Q3244512',
        name: 'Harry Potter',
        description: 'Title character of the fantasy book and film series by J. K. Rowling',
        entity_type: 'fictional_character',
        category: 'Movies & TV',
        gender: 'male',
        nationality: 'United Kingdom',
        debut_year: 1997,
        is_fictional: 1,
        popularity_rank: 11,
        aliases: ['The Boy Who Lived', 'Harry James Potter'],
        attributes: {
            is_real: '0',
            is_male: '1',
            is_female: '0',
            is_anime: '0',
            from_uk: '1',
            has_superpowers: '1',
            uses_magic: '1',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q4847957',
        name: 'Walter White',
        description: 'Protagonist of the American TV series Breaking Bad',
        entity_type: 'fictional_character',
        category: 'Movies & TV',
        gender: 'male',
        nationality: 'United States',
        debut_year: 2008,
        is_fictional: 1,
        popularity_rank: 14,
        aliases: ['Heisenberg'],
        attributes: {
            is_real: '0',
            is_male: '1',
            is_female: '0',
            is_anime: '0',
            from_usa: '1',
            has_superpowers: '0',
            is_scientist: '1',
            debut_after_2015: '0'
        }
    },

    // 9. Cartoons
    {
        wikidata_id: 'Q11934',
        name: 'Mickey Mouse',
        description: 'Cartoon character created by Walt Disney and Ub Iwerks',
        entity_type: 'fictional_character',
        category: 'Cartoons',
        gender: 'male',
        nationality: 'United States',
        debut_year: 1928,
        is_fictional: 1,
        popularity_rank: 13,
        aliases: ['Mickey'],
        attributes: {
            is_real: '0',
            is_male: '1',
            is_female: '0',
            is_cartoon: '1',
            from_usa: '1',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q83279',
        name: 'SpongeBob SquarePants',
        description: 'Title character of the animated series SpongeBob SquarePants',
        entity_type: 'fictional_character',
        category: 'Cartoons',
        gender: 'male',
        debut_year: 1999,
        is_fictional: 1,
        popularity_rank: 17,
        aliases: ['SpongeBob'],
        attributes: {
            is_real: '0',
            is_male: '1',
            is_female: '0',
            is_cartoon: '1',
            from_usa: '1',
            debut_after_2015: '0'
        }
    },

    // 10. Video Games
    {
        wikidata_id: 'Q12379',
        name: 'Mario',
        description: 'Fictional character created by Shigeru Miyamoto and Nintendo mascot',
        entity_type: 'fictional_character',
        category: 'Video Games',
        gender: 'male',
        nationality: 'Japan',
        debut_year: 1981,
        is_fictional: 1,
        popularity_rank: 18,
        aliases: ['Super Mario', 'Jumpman'],
        attributes: {
            is_real: '0',
            is_male: '1',
            is_female: '0',
            is_video_game: '1',
            is_anime: '0',
            from_japan: '1',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q223695',
        name: 'Lara Croft',
        description: 'Main protagonist of the Tomb Raider video game series',
        entity_type: 'fictional_character',
        category: 'Video Games',
        gender: 'female',
        nationality: 'United Kingdom',
        debut_year: 1996,
        is_fictional: 1,
        popularity_rank: 22,
        aliases: ['Tomb Raider'],
        attributes: {
            is_real: '0',
            is_male: '0',
            is_female: '1',
            is_video_game: '1',
            from_uk: '1',
            debut_after_2015: '0'
        }
    },

    // 11. Superheroes & Villains
    {
        wikidata_id: 'Q7982',
        name: 'Spider-Man',
        description: 'Marvel Comics superhero created by Stan Lee and Steve Ditko',
        entity_type: 'fictional_character',
        category: 'Superheroes & Villains',
        gender: 'male',
        nationality: 'United States',
        debut_year: 1962,
        is_fictional: 1,
        popularity_rank: 19,
        aliases: ['Peter Parker', 'Spidey', 'Web-Slinger'],
        attributes: {
            is_real: '0',
            is_male: '1',
            is_female: '0',
            is_superhero: '1',
            is_villain: '0',
            has_superpowers: '1',
            marvel_universe: '1',
            dc_universe: '0',
            wears_mask: '1',
            from_usa: '1',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q2695156',
        name: 'Batman',
        description: 'DC Comics superhero created by Bob Kane and Bill Finger',
        entity_type: 'fictional_character',
        category: 'Superheroes & Villains',
        gender: 'male',
        nationality: 'United States',
        debut_year: 1939,
        is_fictional: 1,
        popularity_rank: 21,
        aliases: ['Bruce Wayne', 'Dark Knight', 'Caped Crusader'],
        attributes: {
            is_real: '0',
            is_male: '1',
            is_female: '0',
            is_superhero: '1',
            is_villain: '0',
            has_superpowers: '0',
            marvel_universe: '0',
            dc_universe: '1',
            wears_mask: '1',
            from_usa: '1',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q2276627',
        name: 'Thanos',
        description: 'Marvel Comics supervillain created by Jim Starlin',
        entity_type: 'fictional_character',
        category: 'Superheroes & Villains',
        gender: 'male',
        debut_year: 1973,
        is_fictional: 1,
        popularity_rank: 23,
        aliases: ['The Mad Titan'],
        attributes: {
            is_real: '0',
            is_male: '1',
            is_female: '0',
            is_superhero: '0',
            is_villain: '1',
            has_superpowers: '1',
            marvel_universe: '1',
            dc_universe: '0',
            debut_after_2015: '0'
        }
    },

    // 12. Historical & Notable People
    {
        wikidata_id: 'Q937',
        name: 'Albert Einstein',
        description: 'German-born theoretical physicist who developed the theory of relativity',
        entity_type: 'human',
        category: 'Historical & Notable',
        gender: 'male',
        nationality: 'Germany',
        birth_date: '1879-03-14',
        death_date: '1955-04-18',
        debut_year: 1905,
        is_fictional: 0,
        popularity_rank: 24,
        aliases: ['Einstein'],
        attributes: {
            is_real: '1',
            is_male: '1',
            is_female: '0',
            is_scientist: '1',
            is_historical: '1',
            is_alive: '0',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q1001',
        name: 'Mahatma Gandhi',
        description: 'Indian lawyer, anti-colonial nationalist, and political ethicist',
        entity_type: 'human',
        category: 'Historical & Notable',
        gender: 'male',
        nationality: 'India',
        birth_date: '1869-10-02',
        death_date: '1948-01-30',
        is_fictional: 0,
        popularity_rank: 25,
        aliases: ['Mohandas Karamchand Gandhi', 'Bapu'],
        attributes: {
            is_real: '1',
            is_male: '1',
            is_female: '0',
            from_india: '1',
            is_historical: '1',
            is_alive: '0',
            debut_after_2015: '0'
        }
    },
    {
        wikidata_id: 'Q7186',
        name: 'Marie Curie',
        description: 'Polish-French physicist and chemist who conducted pioneering research on radioactivity',
        entity_type: 'human',
        category: 'Historical & Notable',
        gender: 'female',
        nationality: 'Poland',
        birth_date: '1867-11-07',
        death_date: '1934-07-04',
        is_fictional: 0,
        popularity_rank: 26,
        aliases: ['Maria Skłodowska-Curie', 'Madame Curie'],
        attributes: {
            is_real: '1',
            is_male: '0',
            is_female: '1',
            is_scientist: '1',
            is_historical: '1',
            is_alive: '0',
            debut_after_2015: '0'
        }
    }
];

module.exports = {
    categories,
    attributes,
    entities
};
