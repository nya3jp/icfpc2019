# Sound! TypeSystem

## Members

- bakaming
- chiro
- fuqinho
- gusmachine
- nya3jp
- phoenixstarhiro
- shunsakuraba
- tanakh
- yuizumi

# Wrappy Solvers

We developed two types of solvers. They are found in `tanakh-solver` and `fuqinho-solver`.

## tanakh-solver

Requires [Rust 1.35+][rust].

[rust]: https://www.rust-lang.org/

~~~~bash
$ cd tanakh-solver
$ cargo build --release
$ cargo run -- --help
~~~~

This solver is based on BFS, with plenty of hacky heuristics:

*   Give the first priority to cloning.
*   Give priority to filling small isolated areas.
*   Get clones well spread out.
*   Manipulators are expanded to the left and right.
*   Use randomness to "improve" the score.
*   Some parameters were deteremined using simulated annealing.

## fuqinho-solver

Requires the [bazel] build system and g++ (GCC). Uses the standard input and output.

[bazel]: https://bazel.build/

~~~~bash
$ bazel run -c opt fuqinho-solver:solve-task2
~~~~

This solver considers all cells within five steps that are not to be painted by another wrappy. It calculates the size of "islands" for each cell; the smallest island takes the priority. Then it uses the beam search to determine the steps to cover the island and uses the number of painted cells, with heuristical weights (e.g. to give higher score to borders).

# Puzzle Solvers

We have three puzzle solvers. They are found in `fuqinho`, `gusmachine/puzzle`, and `yuizumi/puzzle`. All solvers use the standard input and output.

They are more or less similar: just try to "evacuate" from prohibited cells (oSqs). We have multiple solvers as they can sometimes fail for imperfect logic.

## fuqinho

Requires clang++.

~~~~bash
$ clang++ solver-puzzle.cc
$ ./a.out
~~~~

## gusmachine

Requires Python 3.6+. This is a modification of the yuizumi's below.

~~~~bash
$ python3 gusmachine/puzzle/solve-puzzle
~~~~

## yuizumi

Requires Python 3.6+.

~~~~bash
$ python3 yuizumi/puzzle/solve-puzzle
~~~~

# Human Solvers

Some wrappy tasks have been solved by humans. The human-playable simulator can be found in `bakaming/simu`.

# Buyer

LAM allocation for prob-XXX.buy was deteremind using the program in `bakaming/score_calc/calc_index.cpp`. It was run through `metasolver/run.sh`; see this script for details.

We estimated the actual best score to be (0.9 * S), where S is the best score from our solvers (using any boosters), for each problem. Based on this estimate, we calculate the score for each problem, the benefit for each combination of boosters, then maximize the total benefit (using dynamic programming).

# Feedback

You must have already realized... but four hours were way too short to implement anything.

# Self-nomination for Judges' Prize

This year's judges' prize should go to the following teams (except those taking other prizes) for the fact they made the impossible possible:

*   Unagi
*   All your lambda are belong to us
*   Sound! TypeSystem
*   1kg cheese
