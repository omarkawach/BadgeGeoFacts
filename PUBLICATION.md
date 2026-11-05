# Publication safeguards

Only the audited public project files belong in this repository. Device-specific recovery data stays local under the ignored `private/` directory.

## Private data

`private/backups/` contains local recovery files. These can include photos, contacts and Wi-Fi credentials. The ignore rules exclude them from Git. Permissions were restricted in the development checkout; use the README directory-permission commands for new local backups. `.gitignore` also excludes credential/config dumps, keys, environment files, logs and unapproved flash images.

`.gitignore` does not protect a file that was force-added or already tracked. The local pre-commit hook therefore audits the **entire Git index**, rejecting private paths, unexpected files, suspicious credential patterns and unsafe release images. It reports filenames/rules without printing secret values.

Only the root `firmware.bin` and `application.bin` are permitted binary images. The guard verifies their hashes, their matching application bytes, blank NVS/PHY settings and blank trailing data. Private device dumps must never replace these files.

## Review changes before publishing

```sh
git status --short
git diff --cached --stat
git ls-files private backups
python3 tools/check_publication.py
```

`git ls-files private backups` should print nothing. Run `git check-ignore -v private/backups/original-badge.bin` to verify the backup ignore rule. The hook is enabled in the development checkout. Git does not transfer local configuration when cloning: run `git config core.hooksPath .githooks` in a new clone to enable automatic pre-commit checks.

The checker uses a conservative publication allowlist. When adding a new public directory or release asset, review it and update the allowlist intentionally. Do not bypass the hook to add private device data. The guard is a local aid; still review the staged files before a future commit or push.

## Reproduce checks

```sh
python3 -m pip install -r requirements-dev.txt
python3 tools/check_display.py
python3 tools/check_projection.py
python3 tools/check_schedule.py
python3 tools/check_publication.py
```

A future clone can enable the local hook with `git config core.hooksPath .githooks`. No automated workflow in this repository uploads releases or publishes a site. Run the local checks before future commits or pushes.
