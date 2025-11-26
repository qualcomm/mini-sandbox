# Contributing to mini-sandbox

Hi there!
We’re thrilled that you’d like to contribute to this project.
Your help is essential for keeping this project great and for making it better.

## Branching Strategy

In general, contributors should develop on branches based off of `main` and pull requests should be made against `main`.

## Submitting a pull request

1. [Fork](https://github.com/qualcomm/mini-sandbox) and clone the repository.

    ```bash
    git clone https://github.com/<username>/mini-sandbox.git
    ```

1. Create a new branch based on `main`:

    ```bash
    git checkout -b <my-branch-name> main
    ```

1. Create an upstream `remote` to make it easier to keep your branches up-to-date:

    ```bash
    git remote add upstream https://github.com/qualcomm/mini-sandbox.git
    ```

1. Make your changes, add tests, and make sure the tests still pass.
1. Commit your changes using the [DCO](https://developercertificate.org/). You can attest to the DCO by commiting with the **-s** or **--signoff** options or manually adding the "Signed-off-by":

    ```bash
    git commit -s -m "Really useful commit message"`
    ```

1. After committing your changes on the topic branch, sync it with the upstream branch:

    ```bash
    git pull --rebase upstream main
    ```

1. Push to your fork.

    ```bash
    git push -u origin <my-branch-name>
    ```

    The `-u` is shorthand for `--set-upstream`. This will set up the tracking reference so subsequent runs of `git push` or `git pull` can omit the remote and branch.

1. [Submit a pull request](https://github.com/qualcomm/mini-sandbox/pulls) from your branch to `main`.
1. Pat yourself on the back and wait for your pull request to be reviewed.


