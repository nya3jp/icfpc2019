const fs = require('fs');
const process = require('process');
const puppeteer = require('puppeteer');
const tmp = require('tmp');

class Evaluator {
    constructor() {
        this.lazyBrowser_ = puppeteer.launch({args: ['--no-sandbox']});
    }

    async close() {
        const browser = await this.lazyBrowser_;
        await browser.close();
    }

    async run(task_path, solution_path) {
        const browser = await this.lazyBrowser_;
        const page = await browser.newPage();
        try {
            await page.goto("file://" + process.cwd() + "/solution_checker.html");
            const task = await page.$("#submit_task");
            task.uploadFile(task_path);
            const taskUploaded = await Evaluator.waitOutput_(page, "Uploading task description...");
            if (taskUploaded !== "Done uploading task description") {
                throw new Error("Failed to load task: " + taskUploaded);
            }
            const solution = await page.$("#submit_solution");
            solution.uploadFile(solution_path);
            const solutionUploaded = await Evaluator.waitOutput_(page, "Uploading solution file...");
            if (solutionUploaded !== "Done uploading solution") {
                throw new Error("Failed to load solution: " + solutionUploaded);
            }

            await page.click("#execute_solution");
            return await Evaluator.waitOutput_(page, "Pre-processing and validating the task...");
        } finally {
            await page.close();
        }
    }

    static async waitOutput_(page, pending_msg) {
        await page.waitForFunction('document.getElementById("output").textContent != "' + pending_msg + '"');
        // It seems like puppeteer has some timing issue so even if textContent
        // is changed in browser context, there could be a small delay until the
        // change is propagated enough. Poll until it is done.
        for (let i = 0; i < 100; ++i) {
            const uploaded = await page.$eval(
                "#output", item => { return item.textContent; });
            if (uploaded !== pending_msg) {
                return uploaded;
            }
            await page.waitFor(10);
        }
        throw new Error('#output was not set')
    }
}

const evaluator = new Evaluator();

exports.validate = async function(req, res) {
    const { task, solution } = req.body;
    if (!task || !solution) {
        res.status(400).send('Required parameters not set');
        return
    }
    const taskFile = tmp.fileSync();
    const solutionFile = tmp.fileSync();
    try {
        fs.writeFileSync(taskFile.name, task);
        fs.writeFileSync(solutionFile.name, solution);
        const result = await evaluator.run(taskFile.name, solutionFile.name);
        res.status(200).send(result);
    } catch (e) {
        res.status(500).send(`ERROR: ${e}`);
    } finally {
        taskFile.removeCallback();
        solutionFile.removeCallback();
    }
};

if (!process.env["FUNCTION_NAME"]) {
    if (process.argv.length !== 4) {
        console.error("Usage: nodejs validator.js [task] [solution]");
        return;
    }
    (async () => {
        try {
            console.log(await evaluator.run(process.argv[2], process.argv[3]));
        } catch (e) {
            console.error(e);
        } finally {
            await evaluator.close();
        }
    })();
}
