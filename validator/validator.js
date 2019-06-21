const puppeteer = require('puppeteer');
const process = require('process');

if (process.argv.length != 4) {
    console.error("Usage: nodejs validator.js [task] [solution]");
    return;
}

const waitOutput = async (page, pending_msg) => {
    await page.waitForFunction('document.getElementById("output").textContent != "' + pending_msg + '"');
    // It seems like puppeteer has some timing issue so even if textContent
    // is changed in browser context, there could be a small delay until the
    // change is propagated enough. Poll until it is done.
    for (var i = 0; i < 10; ++i) {
        const uploaded = await page.$eval(
            "#output", item => { return item.textContent; });
        if (uploaded != pending_msg) {
            return uploaded;
        }
        await page.waitFor(10);
    }
    return null;
}

(async () => {
    const browser = await puppeteer.launch();
    try {
        const page = await browser.newPage();
        await page.goto("file://" + process.cwd() + "/solution_checker.html");
        const task = await page.$("#submit_task");
        task.uploadFile(process.argv[2]);
        const task_uploaded = await waitOutput(page, "Uploading task description...");
        if (task_uploaded != "Done uploading task description") {
            console.error("Failed to load task: " + task_uploaded);
            return;
        }
        const solution = await page.$("#submit_solution");
        solution.uploadFile(process.argv[3]);
        const solution_uploaded = await waitOutput(page, "Uploading solution file...");
        if (solution_uploaded != "Done uploading solution") {
            console.error("Failed to load solution: " + solution_uploaded);
            return;
        }

        await page.click("#execute_solution");
        const result = await waitOutput(page, "Pre-processing and validating the task...");
        console.log(result);
    } finally {
        await browser.close();
    }
})();
