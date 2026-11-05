const { AkinatorClient } = require("akinator-client");

async function inspect() {
    const aki = new AkinatorClient({
        retries: 1
    });

    try {
        await aki.start();
        console.log("Game started!");
    } catch (error) {
        console.log("ERROR:", error.message);

        console.log("\nAdditional error information:");
        console.dir(error, { depth: 5 });

        console.log("\nClient state:");
        console.dir(aki, { depth: 2 });
    }
}

inspect();