# Component and project files

Irritator stores its models in two JSON formats:

- a **component file** (`.irt`) describes one component: its ports, its
  external sources and its content (a generic component, a grid, a graph, a
  hierarchical state machine or a simulation component);
- a **project file** (`.pirt`) instantiates a top component and stores the
  simulation bounds, the global parameters and the observers.

Both are written by `irt::save_component` / `irt::save_project` and read by
`irt::load_component` / `irt::load_project` (`lib/include/irritator/archiver.hpp`,
`lib/src/json.cpp`).

## General rules

- The files are UTF-8 JSON. `NaN`, `Infinity` and `-Infinity` are accepted as
  numbers: a `timeout` is often `Infinity`.
- The order of the members matters in a few objects. The reader processes the
  members in file order and some members need the result of a previous one.
  The writer always uses the order given in this document; keep it when you
  write a file by hand. The constraints are listed in each section.
- Most objects ignore unknown members. Some objects, such as most dynamics
  parameters, refuse them.
- Strings are bounded: 31 characters for a name, 7 for a port name and 63 for
  a directory or a file name. A longer string is an error.

### Referencing another component

A component never embeds another one: it references a component file through
three members.

| Member      | Description                                                    |
|-------------|----------------------------------------------------------------|
| `path`      | Name of the registered path (as configured in the settings).   |
| `directory` | Directory inside the registered path.                          |
| `file`      | File name, for example `cell.irt`.                             |

When `path` is absent or names no registered path, the directory is searched
in every registered path. This makes a component portable between the
`system` and `user` paths, as the files in `examples/` do.

A referenced component must be read before the component that references it.
`modeling::fill_components()` reads every component file of the registered
paths and retries those whose dependencies are not read yet, until no more
component can be read.

A *child component* reference adds its type:

```json
{ "component-type": "simple", "path": "user", "directory": "diffusion", "file": "cell.irt" }
```

`component-type` is one of `none`, `simple` (generic), `grid`, `graph`, `hsm`
or `simulation`. With `none`, the other members are absent.

## Component file (`.irt`)

```json
{
  "name": "cell",
  "sources": [ ... ],
  "colors": [0.3, 0.9, 0.2, 1.0],
  "x": [ ... ],
  "y": [ ... ],
  "type": "simple",
  ...
}
```

| Member    | Type             | Description                                                   |
|-----------|------------------|---------------------------------------------------------------|
| `name`    | string           | Name of the component.                                        |
| `sources` | array, optional  | External sources used by the models, see [Sources](#sources). |
| `colors`  | array of 4 reals | RGBA color in the editor.                                     |
| `x`       | array, optional  | Input ports, see [Ports](#ports).                             |
| `y`       | array, optional  | Output ports.                                                 |
| `type`    | string           | `none`, `simple`, `grid`, `graph`, `hsm` or `simulation`.     |

The members after `type` depend on the type and are described below.

**Order:** `sources`, `x` and `y` must precede `type`. The content is read as
soon as `type` is met, and it refers to the sources and ports.

### Ports

```json
"x": [ { "name": "in", "type": "classic", "x": 0.0, "y": 0.0 } ]
```

| Member | Description                                                  |
|--------|--------------------------------------------------------------|
| `name` | Port name, unique in `x` (respectively `y`).                 |
| `type` | `classic`, `sum` or `wsum`.                                  |
| `x`, `y` | Position of the port in the editor.                        |

### Sources

Each source has an identifier local to the file. The models refer to it with a
`source-id` member.

```json
"sources": [
  { "id": 0, "name": "rates", "type": "constant", "parameters": [1.0, 2.0, 3.0] },
  { "id": 1, "name": "data", "type": "binary-file", "path": "data.bin" },
  { "id": 2, "name": "text", "type": "text-file", "path": "data.txt" },
  { "id": 3, "name": "noise", "type": "random", "distribution": "normal", "mean": 0.0, "stddev": 1.0 }
]
```

| Source type   | Members                                                             |
|---------------|---------------------------------------------------------------------|
| `constant`    | `parameters`: array of reals.                                       |
| `binary-file` | `path`: file name in the directory of the component.                |
| `text-file`   | `path`: file name in the directory of the component.                |
| `random`      | `distribution` and its parameters, see the table below.             |

Older files store the distribution of a random source in a second `type`
member; the reader still accepts them.

| Distribution        | Parameters                         |
|---------------------|------------------------------------|
| `uniform_int`       | `a`, `b` (integers)                |
| `uniform_real`      | `a`, `b`                           |
| `bernouilli`        | `p`                                |
| `binomial`          | `t` (integer), `p`                 |
| `negative_binomial` | `t` (integer), `p`                 |
| `geometric`         | `p`                                |
| `poisson`           | `mean`                             |
| `exponential`       | `lambda`                           |
| `gamma`             | `alpha`, `beta`                    |
| `weibull`           | `a`, `b`                           |
| `exterme_value`     | `a`, `b`                           |
| `normal`            | `mean`, `stddev`                   |
| `lognormal`         | `m`, `s`                           |
| `chi_squared`       | `n`                                |
| `cauchy`            | `a`, `b`                           |
| `fisher_f`          | `m`, `n`                           |
| `student_t`         | `n`                                |

### Generic component (`"type": "simple"`)

A generic component contains models and child components connected together.

```json
"type": "simple",
"children": [
  { "id": 0, "x": 10.0, "y": 20.0, "name": "integrator", "configurable": false,
    "observable": true, "type": "qss1_integrator",
    "dynamics": { "X": 0.0, "dQ": 0.01 } },
  { "id": 1, "x": 50.0, "y": 20.0, "name": "", "configurable": false,
    "observable": false, "type": "component",
    "component-type": "simple", "path": "user", "directory": "lib", "file": "sum.irt" }
],
"connections": [
  { "type": "internal", "source": 0, "port-source": 0, "destination": 1, "port-destination": "in" },
  { "type": "input", "port": "in", "destination": 0, "port-destination": 0 },
  { "type": "output", "port": "out", "source": 0, "port-source": 0 }
]
```

**Children**

| Member         | Description                                                         |
|----------------|---------------------------------------------------------------------|
| `id`           | Identifier local to the file, used by `connections`.                |
| `x`, `y`       | Position in the editor.                                             |
| `name`         | Name of the child. A configurable or observable child always gets a unique name, used by the project files to reach it. |
| `configurable` | The project can override the parameters of this model.              |
| `observable`   | The project can observe this model.                                 |
| `type`         | A dynamics name (see [Dynamics](#dynamics)) or `component`.         |
| `dynamics`     | For a model: its parameters, an object, possibly empty.             |

A child of type `component` carries the members of a child component
reference (`component-type`, `path`, `directory`, `file`).

**Connections**

The port of a model is its index (an integer). The port of a child component
is the name of one of its ports (a string).

| `type`        | Members                                                                 |
|---------------|-------------------------------------------------------------------------|
| `internal`    | `source`, `port-source`, `destination`, `port-destination`.             |
| `input`       | `port` (input port of this component), `destination`, `port-destination`. |
| `output`      | `port` (output port of this component), `source`, `port-source`.        |
| `input-pack`  | See [Connection packs](#connection-packs).                              |
| `output-pack` | See [Connection packs](#connection-packs).                              |

**Order:** `children` must precede `connections`.

### Grid component (`"type": "grid"`)

A grid repeats child components on `rows` × `columns` cells and connects the
neighbors automatically.

```json
"type": "grid",
"rows": 10,
"columns": 10,
"in-connection-type": 1,
"out-connection-type": 1,
"options": "none",
"neighbors": "four",
"children": [ { "component-type": "simple", "path": "user", "directory": "diffusion", "file": "cell.irt" }, ... ],
"connections": [
  { "type": "input", "row": 0, "col": 0, "id": "in", "x": "in" },
  { "type": "output", "row": 9, "col": 9, "id": "out", "y": "out" }
]
```

| Member                | Description                                                         |
|-----------------------|---------------------------------------------------------------------|
| `rows`, `columns`     | Size of the grid, in [1, 1024).                                     |
| `in-connection-type`, `out-connection-type` | How the neighbors are connected: `0` a single port `in`/`out`, `1` ports named after the cardinal points (`N`, `S`, `W`, `E`, `NE`…), `2` ports named after the neighborhood coordinates. |
| `options`             | Optional: `none`, `row_cylinder`, `column_cylinder` or `torus` to wrap the rows, the columns or both. |
| `neighbors`           | Optional: `four` or `eight`, the neighborhood of the automatic connections. |
| `children`            | Exactly `rows` × `columns` child component references, row by row. |
| `connections`         | Connections between the ports of this component and the cells.      |

Grid connections:

| `type`        | Members                                                                  |
|---------------|--------------------------------------------------------------------------|
| `input`       | `row`, `col`, `id` (input port of the cell), `x` (input port of the grid). |
| `output`      | `row`, `col`, `id` (output port of the cell), `y` (output port of the grid). |
| `input-pack`, `output-pack` | See [Connection packs](#connection-packs).                 |

**Order:** `rows` and `columns` must precede `children`, and `children` must
precede `connections`.

### Graph component (`"type": "graph"`)

A graph places child components on the vertices of a generated or imported
graph.

```json
"type": "graph",
"graph-type": "small-world",
"probability": 0.03,
"k": 6,
"nodes": 32,
"children": [ { "component-type": "simple", "path": "user", "directory": "lib", "file": "node.irt" }, ... ],
"connections": [
  { "type": "input", "v": 0, "id": "in", "x": "in" },
  { "type": "output", "v": 3, "id": "out", "y": "out" }
]
```

`graph-type` selects the graph and its parameters:

| `graph-type`  | Members                                                                 |
|---------------|-------------------------------------------------------------------------|
| `scale-free`  | `alpha` (> 0), `beta` (> 0), `nodes`, `children`.                       |
| `small-world` | `probability` (> 0), `k` (≥ 1), `nodes`, `children`.                    |
| `dot-file`    | `path`, `dir`, `file`: a Graphviz file. The vertices and their components come from this file, so `children` is absent. |

The scale-free and small-world graphs are generated again when the file is
read. `children` gives the component of each vertex, in vertex order.

Graph connections:

| `type`        | Members                                                                   |
|---------------|---------------------------------------------------------------------------|
| `input`       | `v` (vertex index), `id` (input port of the vertex component), `x` (input port of the graph). |
| `output`      | `v`, `id` (output port of the vertex component), `y` (output port of the graph). |
| `input-pack`, `output-pack` | See [Connection packs](#connection-packs).                  |

**Order:** `graph-type` must precede `connections`.

### Connection packs

A pack connects a port of a grid, graph or generic component to a port of the
children built from a given component, without listing the children one by
one. The packs are written in the `connections` array of these components.

```json
{ "type": "input-pack", "port": "in",
  "component-type": "simple", "path": "user", "directory": "lib", "file": "cell.irt",
  "child-port": "in" }
```

| Member       | Description                                                     |
|--------------|-----------------------------------------------------------------|
| `port`       | Port of this component: input for `input-pack`, output for `output-pack`. |
| `child-port` | Port of the child component, of the same direction.             |

The child component is given by a child component reference.

### Hierarchical state machine (`"type": "hsm"`)

```json
"type": "hsm",
"states": [
  { "id": 0, "name": "idle",
    "enter": { "var-1": 0, "var-2": 0, "type": 0 },
    "exit": { "var-1": 0, "var-2": 0, "type": 0 },
    "if": { "var-1": 0, "var-2": 0, "type": 0 },
    "else": { "var-1": 0, "var-2": 0, "type": 0 },
    "condition": { "var-1": 0, "var-2": 0, "type": 1, "port": 1, "mask": 1 },
    "if-transition": 1, "else-transition": 255, "super-id": 255, "sub-id": 255,
    "x": 0.0, "y": 0.0 }
],
"top": 0,
"i1": 0, "i2": 0, "r1": 0.0, "r2": 0.0,
"timeout": Infinity,
"source-id": 0,
"constants": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
```

| Member      | Description                                                             |
|-------------|-------------------------------------------------------------------------|
| `states`    | The states in use. A state `id` is in [0, 254).                         |
| `top`       | Identifier of the top state.                                            |
| `i1`, `i2`, `r1`, `r2`, `timeout` | Default values of the parameters of the machine.  |
| `source-id` | Optional external source of the machine.                                |
| `constants` | At most 8 reals, used by the actions and conditions.                    |

A state:

| Member                | Description                                                     |
|-----------------------|-----------------------------------------------------------------|
| `id`                  | Index of the state.                                             |
| `name`                | Optional name.                                                  |
| `enter`, `exit`, `if`, `else` | Actions, see below.                                     |
| `condition`           | Condition that selects the `if` or the `else` branch.           |
| `if-transition`, `else-transition` | Next state, 255 for none.                          |
| `super-id`, `sub-id`  | Parent and first child states, 255 for none.                    |
| `x`, `y`              | Position in the editor.                                         |

An action or a condition has `var-1`, `var-2` and `type`, three integers
whose values are the enumerations `hierarchical_state_machine::variable`
(< 21), `action_type` (< 16) and `condition_type` (< 9) in `core.hpp`. A
condition on the input ports adds `port` and `mask`. When `var-2` is an integer
or a real constant, `constant-i` or `constant-r` gives its value.

### Simulation component (`"type": "simulation"`)

A simulation component runs a project several times and exposes its results.

```json
"type": "simulation",
"path": "user", "directory": "projects", "file": "lotka.pirt",
"factors": [
  { "name": "rate", "type": "single", "access": ["prey", "integrator"], "value": 1.5 },
  { "name": "init", "type": "fixed", "access": ["predator", "integrator"], "values": [1.0, 2.0] },
  { "name": "noise", "type": "random", "access": ["prey", "gain"],
    "values": { "type": "normal", "mean": 0.0, "stddev": 1.0 }, "count": 10 }
],
"selections": [
  { "name": "prey", "access": ["prey", "integrator"], "criteria": "max" }
],
"objective": {
  "method": "simple",
  "type": "maximize",
  "simple-parameters": { "primary": "prey" }
}
```

`path`, `directory` and `file` reference the project file. The project must
be readable when the component is read.

**Factors** modify a parameter of a model of the project, reached by `access`
(see [Access paths](#access-paths)).

| `type`                                        | Members                                       |
|-----------------------------------------------|-----------------------------------------------|
| `single`, `single-add`, `single-mult`         | `value`: a real.                              |
| `fixed`, `fixed-add`, `fixed-mult`            | `values`: an array of reals.                  |
| `random`, `random-add`, `random-mult`         | `values`: a distribution, written as a random source with a `type` member; `count`: the number of draws. |

**Selections** observe a model of the project. `criteria` is `min-last`,
`max-last`, `min` or `max`.

**Objective**

| Member   | Description                                                    |
|----------|----------------------------------------------------------------|
| `method` | `epsilon-constrained`, `simple` or `weighted-sum`.             |
| `type`   | `maximize` or `minimize`.                                      |

Then, according to `method`:

- `simple-parameters`: `primary`, the name of the selection to optimize;
- `weighted-sum-parameters`: `norm` (`min-max` or `z-score`), `types` (one
  `maximize` or `minimize` per selection) and `weights` (one real per
  selection);
- `epsilon-constrained-parameters`: `epsilons` (one real per selection),
  `operations` (one of `equal`, `not-equal`, `greater-equal`, `less-equal`
  per selection) and `primary`.

**Order:** `selections` must precede `objective`, since `primary` names a
selection.

### Dynamics

The `type` of a model child is a dynamics name and `dynamics` holds its
parameters. The QSS models exist in three orders with the prefixes `qss1_`,
`qss2_` and `qss3_`, for example `qss2_integrator`.

| Dynamics                                    | Parameters                                     |
|---------------------------------------------|------------------------------------------------|
| `qssN_integrator`                           | `X` (initial value), `dQ` (quantum).           |
| `qssN_wsum_2`, `qssN_wsum_3`, `qssN_wsum_4` | `coeff-0`, `coeff-1`, … one per input.         |
| `qssN_compare`                              | `a-less-b`, `not-a-less-b`: output values.     |
| `qssN_cross`                                | `threshold`, `up`, `bottom`.                   |
| `qssN_filter`                               | `lower-threshold`, `upper-threshold`.          |
| `qssN_power`                                | `n`: the exponent.                             |
| `qssN_gain`                                 | `k`.                                           |
| `qssN_sample_hold`                          | `ts`: the sampling period.                     |
| `qssN_quantizer`                            | `step`.                                        |
| `qssN_integrate_and_fire`                   | `threshold`.                                   |
| `qssN_threshold_crossing`                   | `threshold`.                                   |
| `qssN_pwm`                                  | `period`, `amplitude`.                         |
| `qssN_saturation`, `qssN_dead_zone`         | `lower`, `upper`.                              |
| `qssN_hysteresis`                           | `lower`, `upper`, `out-low`, `out-high`.       |
| `qssN_wrap`                                 | `origin`, `modulo`.                            |
| `counter`                                   | `observation-type`: `event-number`, `last-value` or `sum-values`. |
| `queue`                                     | `ta`: the delay, > 0.                          |
| `dynamic_queue`, `priority_queue`           | `source-id`: the source of the delays.         |
| `generator`                                 | `ta-is-external`, `value-is-external` (booleans), then `source-ta-id` or `sigma`, and `source-value-id` or `value`. |
| `constant`                                  | `value`, `offset` (≥ 0), `type` and, for the `_n` types, `port`. |
| `time_func`                                 | `function` (`time`, `square` or `sin`), `timestep`, `offset`. |
| `hsm_wrapper`                               | `hsm`: a component reference to an `hsm` component; `i1`, `i2`, `r1`, `r2`, `timeout`; `source-id` when the machine uses a source. |
| `simulation_wrapper`                        | `sim`: a component reference to a `simulation` component; `run-type` (`bag`, `complete`, `during`, `time` or `until`); `factors` and `selections`, their numbers. |

The `constant` `type` is `constant`, `incoming_component_all`,
`outcoming_component_all`, `incoming_component_n` or `outcoming_component_n`.
With the last two, `port` names an input (respectively output) port of the
component, created if it does not exist; `type` must precede `port`.

All the other dynamics have no parameter and are written with an empty
`dynamics` object: `qssN_multiplier`, `qssN_max_hold`, `qssN_min_hold`,
`qssN_flipflop`, `qssN_square`, `qssN_sum_2`, `qssN_sum_3`, `qssN_sum_4`,
`qssN_inverse`, `qssN_integer`, `qssN_sin`, `qssN_cos`, `qssN_log`,
`qssN_exp`, `qssN_abs`, `qssN_atan`, `qssN_atan2`, `qssN_division`,
`qssN_maximum`, `qssN_minimum`, `qssN_sigmoid`, `qssN_sign`, `qssN_sqrt`,
`qssN_tan`, `qssN_tanh`, `accumulator_2`, `logical_and_2`, `logical_and_3`,
`logical_or_2`, `logical_or_3`, `logical_invert` and `zero_order_hold`.

## Project file (`.pirt`)

```json
{
  "begin": 0.0,
  "end": 100.0,
  "component-type": "grid",
  "component-path": "user",
  "component-directory": "diffusion",
  "component-file": "grid.irt",
  "parameters": {
    "global": [
      { "name": "init", "access": ["cell-0-0", "integrator"],
        "parameter": { "real": [1.0, 0.01, 0.0, 0.0], "integer": [0, 0, 0, 0] } }
    ]
  },
  "observations": {
    "global": [
      { "name": "plot", "models": [
          { "name": "x", "access": ["cell-0-0", "integrator"],
            "color": [1.0, 0.5, 1.0, 0.0], "type": "line" } ] }
    ],
    "grid": [ { "name": "map", "grid": [], "access": ["cell-0-0", "integrator"] } ],
    "graph": [ { "name": "net", "graph": [], "access": ["node-0", "integrator"] } ]
  }
}
```

| Member       | Description                                                               |
|--------------|---------------------------------------------------------------------------|
| `begin`, `end` | Simulation bounds, 0 and 100 by default.                                |
| `component-type`, `component-path`, `component-directory`, `component-file` | The top component. It must be read before the project. |
| `parameters` | Overridden parameters of the models.                                     |
| `observations` | The observers.                                                          |

### Access paths

An access path reaches a model or a tree node of the project from the top
component. It is an array of unique names: the names of the child components
from the top to the parent of the model, then the name of the model. In a
generic component, the unique name is the name of the child: give a name to
the children to reach, or make them configurable or observable to get one
generated. The cells of a grid and the vertices of a graph get a generated
name. An empty array is the top component itself.

### Parameters

`parameters.global` overrides the parameters of models:

| Member      | Description                                                    |
|-------------|----------------------------------------------------------------|
| `name`      | Name of the parameter set.                                     |
| `access`    | Access path of the model.                                      |
| `parameter` | `real`: 4 reals and `integer`: 4 integers, the raw parameters of the dynamics. |

### Observations

| Member   | Description                                                                    |
|----------|--------------------------------------------------------------------------------|
| `global` | Plots. Each has a `name` and `models`: an array of `{name, access, color, type}`, where `name` is optional, `color` is RGBA and `type` is `none`, `line` or `dash`. |
| `grid`   | Grid observers: `name`, `grid` (access path of the grid tree node) and `access` (the observed model in each cell). |
| `graph`  | Graph observers: `name`, `graph` (access path of the graph tree node) and `access`. |
