const {
    AkinatorClient,
    Languages,
    Themes
} = require("akinator-client");

async function test() {
    console.log("Testing English Akinator...");

    const aki = new AkinatorClient({
        language: "en",
        theme: Themes.Character,
        retries: 1
    });

    try {
        const result = await aki.start();

        console.log("\nSUCCESS!");
        console.log("Question:", result.question);
        console.log("Answers:", result.answers);
        console.log("Progress:", result.progression);

    } catch (error) {
        console.log("\nFAILED!");
        console.log("Message:", error.message);
        console.dir(aki, { depth: 1 });
    }
}

test();