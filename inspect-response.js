const { AkinatorClient } = require("akinator-client");

async function inspect() {
    const aki = new AkinatorClient({
        language: "en",
        retries: 1
    });

    try {
        await aki.start();
        console.log("Game started successfully!");
    } catch (error) {
        console.log("Start failed:", error.message);
        console.log("Akinator URL:", aki._url);
        console.log("Cookies:", aki._cookies);
        console.log("Session:", aki._session);
        console.log("Signature:", aki._signature);

        console.log("\nInspecting HTTP client:");
        console.log("HTTP client available:", typeof aki._got);
    }
}

inspect();