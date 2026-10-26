# Optional TCE helpers

This directory is for local launch/build helpers, not TC gameplay. Production
gameplay sources whose names begin with `tce_` remain under `src/`.

`launch-legacy.sh` preserves the former root `tce` launcher byte for byte. It
expects `./et` in the caller's working directory and the historical `tcetest`
mod installation. It is not included in public DLL/PK3 packages.
