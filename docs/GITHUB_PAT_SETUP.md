# GitHub PAT Setup for MycoBrain Automation

**Date:** 2026-05-21
**Audience:** Anyone (or any Claude session) setting up automated GitHub access to `MycosoftLabs/mycobrain`.

This is the exact permission set needed for fine-grained PATs to write to the repo. If you skip steps here, you'll see `403 "Resource not accessible by personal access token"` on every write call — both `git push` over HTTPS and the REST API.

## The trap that bit us twice on 2026-05-21

Two PATs were generated trying to fix this. Both came out **read-only** because the fine-grained PAT creation UI starts every permission at "No access" or "Read-only" by default — you have to click each dropdown and explicitly change it. If you skip that step, the token "looks" right (it's identified, it can read), but every write call returns the same 403.

Don't trust that a token works just because `GET /user` returns your login. Always run the write probe in the "Verifying the PAT works" section before declaring victory.

## Diagnosis we hit (2026-05-21)

A fine-grained PAT issued for `MycosoftLabs/mycobrain` with default settings could:

| Endpoint | Result | Why |
|----------|--------|-----|
| `GET /user` | 200 | Default `Read` scope on user metadata |
| `GET /repos/MycosoftLabs/mycobrain` | 200 with admin/maintain/push/triage/pull = true | Owner sees their own repo metadata |
| `GET /repos/MycosoftLabs/mycobrain/collaborators/.../permission` | **403** | Needs `Administration: Read` |
| `POST /repos/MycosoftLabs/mycobrain/labels` (create label) | **403** | Needs `Issues: Read and write` |
| `git push origin <branch>` | **403** | Needs `Contents: Read and write` |
| `POST .../pulls` (open PR) | **403** | Needs `Pull requests: Read and write` |

The user-level `repo permissions: admin=true` shown by `GET /repos/...` describes what the **user** can do on the repo, not what the **token** is allowed to do. Fine-grained PATs intersect those two — the token can never exceed the user's grants, but it can be **narrower**.

## The right PAT configuration

Go to https://github.com/settings/personal-access-tokens → "Fine-grained tokens" → New (or edit the existing one).

**Resource owner:** `MycosoftLabs`

**Repository access:** Either "All repositories" (broad) or just `MycosoftLabs/mycobrain` (recommended principle of least privilege).

**Repository permissions** — minimum set for the MycoBrain automation work:

| Permission | Level | Why |
|------------|-------|-----|
| **Contents** | Read and write | `git push`, file commits via API |
| **Pull requests** | Read and write | open / update / merge PRs from chat |
| **Issues** | Read and write | open / triage issues from chat |
| **Workflows** | Read and write | edit `.github/workflows/*.yml` (CI) |
| Metadata | Read | implicit / auto-granted |

**Account permissions** — none needed for repo-scoped work.

**Expiration:** pick a value that matches your rotation cadence. Mycosoft default: 90 days. Calendar a renewal reminder.

## Verifying the PAT works

After saving the token, run from any shell:

```bash
PAT='github_pat_...'

# 1. Identity (read scope)
curl -sS -H "Authorization: Bearer $PAT" https://api.github.com/user | jq .login
#   expected: "nodefather"

# 2. Repo visibility
curl -sS -H "Authorization: Bearer $PAT" https://api.github.com/repos/MycosoftLabs/mycobrain | jq '.full_name, .permissions'

# 3. Write probe — creates a temp label, deletes it
curl -sS -X POST -H "Authorization: Bearer $PAT" -H "Accept: application/vnd.github+json" \
  https://api.github.com/repos/MycosoftLabs/mycobrain/labels \
  -d '{"name":"pat-write-check","color":"00ff00"}'
curl -sS -X DELETE -H "Authorization: Bearer $PAT" \
  https://api.github.com/repos/MycosoftLabs/mycobrain/labels/pat-write-check

# 4. Push probe — push an empty branch
git push https://x-access-token:$PAT@github.com/MycosoftLabs/mycobrain.git \
  $(git rev-parse HEAD):refs/heads/pat-write-check
# then delete it
git push https://x-access-token:$PAT@github.com/MycosoftLabs/mycobrain.git \
  --delete pat-write-check
```

If step 3 or 4 returns 403, the permission set is wrong — go back to the token settings page.

## Where the PAT lives

| Location | Purpose | Notes |
|----------|---------|-------|
| Cowork `claude_desktop_config.json` | GitHub MCP server env var | Set via `scripts/install-github-mcp.ps1` |
| `C:\Users\Owner1\AppData\Roaming\GitHub CLI\hosts.yml` | `gh` CLI | Managed by `gh auth login` — different OAuth token |
| `C:\Users\Owner1\Desktop\api keys.txt` | Reference (Morgan's local) | NOT committed; gitignored by Windows convention |
| Claude memory | Cross-session continuity | See `mycobrain_jetson_ssh.md`-style entry under `github_pat.md` |

**Never** commit the PAT to git, paste it into PR descriptions, or include it in artifacts presented to the user. The temporary `_install-github-mcp-RUN.bat` is `.gitignore`d and should be deleted after one-time use.

## When the PAT is rotated

1. Update the token in https://github.com/settings/personal-access-tokens (regenerate, copy new value)
2. Update Cowork's GitHub MCP env var:
   ```powershell
   .\scripts\install-github-mcp.ps1 -Token <new-PAT> -Restart
   ```
3. Update Claude's memory file `github_pat.md` (Claude will do this on first failed call)
4. (Optional) Rotate any embedded copies in CI / external services

## Classic vs fine-grained tokens

For a single-developer workflow you can also use a classic PAT with `repo` and `workflow` scopes — fewer clicks, broader access. Fine-grained is preferred long-term because:

- Per-repo scoping (lose a token → only one repo affected)
- Clearer audit log
- Owner-approval flows for org-owned repos

If you're hitting too much friction with fine-grained, classic is acceptable — just rotate it more aggressively.
