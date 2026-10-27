# <module> — design note

Header: `<dsa/...>`   Kind: std-faithful | variant of `<std::...>` | no std equivalent
Status: ⬜ 🚧 ✅   Work item: `<item-id>`

## Purpose

What problem this solves, and when you'd reach for it over the alternatives.

## Memory layout

Diagram or description: what lives where, how many allocations, cache lines
touched per common operation.

## Invariants

What is always true between public calls.

## Guarantees

| Operation | Complexity | Exception safety | Invalidates |
|-----------|------------|------------------|-------------|

For variants: **Dropped vs std** (each dropped guarantee, why, what it bought,
with a link to the benchmark row).

## Implementation notes

How the major std implementations do it (libstdc++ / libc++ / MS STL), and
where this one differs and why.

## Performance

Summary of `work/<item-id>/04_measure.md`: where it wins, where it loses,
and the mechanism behind each.

## When to use / when not to

Including at least one critical-systems scenario (bounded latency, no-heap,
determinism).

## Open questions
