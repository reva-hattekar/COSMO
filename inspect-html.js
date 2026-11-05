const { AkinatorClient } = require("akinator-client");

async function inspect() {
    const aki = new AkinatorClient({
        language: "en",
        retries: 1
    });

    try {
        aki._url = "https://en.akinator.com";

        await aki._get(aki._url + "/");

        const response = await aki._post(
            aki._url + "/game",
            {
                sid: 1,
                cm: false
            }
        );

        const html = String(response.body);

        console.log("STATUS:", response.statusCode);
        console.log("HTML LENGTH:", html.length);

        console.log("\nSESSION occurrences:");
        console.log([...html.matchAll(/session/gi)].map(m => ({
            index: m.index,
            text: html.substring(Math.max(0, m.index - 100), m.index + 200)
        })).slice(0, 5));

        console.log("\nSIGNATURE occurrences:");
        console.log([...html.matchAll(/signature/gi)].map(m => ({
            index: m.index,
            text: html.substring(Math.max(0, m.index - 100), m.index + 200)
        })).slice(0, 5));

        console.log("\nINPUT FIELDS:");
        console.log(html.match(/<input[^>]*>/gi)?.slice(0, 20));

        console.log("\nPAGE TITLE:");
        console.log(html.match(/<title[^>]*>.*?<\/title>/is)?.[0]);

    } catch (error) {
        console.error(error);
    }
}

inspect();