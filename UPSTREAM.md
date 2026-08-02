# Upstream

Everything under `vendor/` is copied verbatim from
[tarikbc/hotspot-arcade](https://github.com/tarikbc/hotspot-arcade) (MIT,
Tarik Caramanico). Nothing in `vendor/` is edited here -- see README.

> **Ahead of upstream (temporary).** The pinned commit below is *not yet* in
> `tarikbc/master`: it carries the Kiss Marry Kill game, which is offered
> upstream in [tarikbc/hotspot-arcade#8](https://github.com/tarikbc/hotspot-arcade/pull/8)
> and, until that merges, lives on the `game-kmk` branch of the
> [genkigenki/hotspot-arcade](https://github.com/genkigenki/hotspot-arcade)
> fork. Once the PR is merged this re-pins to Tarik's official commit and the
> line above holds again.

| | |
| --- | --- |
| commit | `037da437e099eca3590dcc08c8c1bc2a7e443a1a` |
| describe | `v1.3.0-2-g037da43` |
| engine | `vendor/engine/` -- ha_proto.h, ha_json.h, ha_games.h |
| web bundle | `vendor/web/` -- 1 file(s) |
| content packs | `vendor/packs/` -- 32 pack(s) |
| async libs | `vendor/libs/` -- AsyncTCP, ESPAsyncWebServer (third-party, own LICENSE files) |

Refresh with:

```sh
node tools/sync-upstream.mjs [path-to-upstream-clone]
node tools/gen-assets.mjs
```

`git diff vendor/` after a sync is exactly the upstream change.
