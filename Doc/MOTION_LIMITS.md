# Simulation-only motion limits

The UI edits six axes: X/Y/Z use millimeters; RX/RY/RZ use degrees. Each axis stores minimum, maximum, maximum excursion, maximum step, maximum requested speed, and maximum load. The server rejects missing/non-finite values, wrong units, invalid min/max ordering, nonpositive caps, excursion larger than the position span, out-of-envelope targets, excessive steps/speeds, and excessive load.

Defaults are demonstration values and are not machine-specific physical limits. A nonempty machine profile and `simulation_only: true` are required. Limits persist to local ignored `config/motion_limits.json`. The simulator updates only an in-memory position vector. No physical move command, EtherCAT, serial, piezo, or servo code is present.
