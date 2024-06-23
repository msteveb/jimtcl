# Contributing to Jim

Please take a moment to review this document in order to make the contribution
process easy and effective for everyone involved.

Following these guidelines helps to communicate that you respect the time of
the developers managing and developing this open source project. In return,
they should reciprocate that respect in addressing your issue, assessing
changes, and helping you finalize your pull requests.

## Bug reports and Feature requests

The [issue tracker](https://github.com/msteveb/jimtcl/issues) is the preferred channel for bug reports
and features requests.

## Discussions 

If you wish to open a broader topic for discussion, consider using a [discussion topic](https://github.com/msteveb/jimtcl/discussions)

## Pull requests

[Pull requests](https://github.com/msteveb/jimtcl/pulls) are always welcome

If you have never created a pull request before, [Here is a great tutorial](https://egghead.io/series/how-to-contribute-to-an-open-source-project-on-github)
on how to create a pull request.

1. [Fork](http://help.github.com/fork-a-repo/) the project, clone your fork,
   and configure the remotes:

    ```
    # Clone your fork of the repo into the current directory
    git clone https://github.com/<your-username>/<repo-name>
    # Navigate to the newly cloned directory
    cd <repo-name>
    # Assign the original repo to a remote called "upstream"
    git remote add upstream https://github.com/msteveb/jimtcl.git
    ```

2. If you cloned a while ago, get the latest changes from upstream:

   ```
   git checkout master
   git pull upstream master
   ```

3. Create a new topic branch (off the main project development branch) to
   contain your feature, change, or fix:

   ```bash
   git checkout -b <topic-branch-name>
   ```

4. Make sure to update, or add to the tests when appropriate.
   Run `make test` to check that all tests pass after you've made changes.

5. If you added or changed a feature, please add documentation to jim_tcl.txt.

6. Push your topic branch up to your fork:

   ```bash
   git push origin <topic-branch-name>
   ```

7. [Open a Pull Request](https://help.github.com/articles/using-pull-requests/)
    with a clear title and description.
