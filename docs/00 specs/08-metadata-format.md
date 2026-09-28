## Metadata format

App-only data (group, color, notes) is stored as structured comments immediately above the `Host` line in the managed file:

```
# sshmanager: group="Production" color="#d33" note="Primary DB"
Host prod-db
    HostName 10.0.0.12
    User admin
```

Unknown metadata keys must be preserved on write. Never store secrets in metadata.

---

**Related specs:**

- [SSH config rules (critical)](07-ssh-config-rules.md)
- [Security](10-security.md)
