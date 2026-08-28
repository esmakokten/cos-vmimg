#!/usr/bin/env python3
"""Translate a recipe TOML into a make fragment.

Usage: recipe2mk.py <recipe.toml>   (writes the fragment to stdout)

Keeping the parsing here rather than in shell means a malformed recipe fails
with a real error instead of silently producing an empty variable, which is how
you end up with an image that boots to nothing.
"""
import sys, tomllib, pathlib

ALLOWED = {"description", "init", "programs", "modules", "fragment"}

def main() -> int:
    path = pathlib.Path(sys.argv[1])
    with path.open("rb") as fh:
        recipe = tomllib.load(fh)

    unknown = set(recipe) - ALLOWED
    if unknown:
        sys.exit(f"{path}: unknown key(s): {', '.join(sorted(unknown))}")
    if "init" not in recipe:
        sys.exit(f"{path}: missing required key 'init'")

    out = [
        f"RECIPE_DESC := {recipe.get('description', '')}",
        f"RECIPE_INIT := {recipe['init']}",
        f"RECIPE_PROGRAMS := {' '.join(recipe.get('programs', []))}",
        f"RECIPE_MODULES := {' '.join(recipe.get('modules', []))}",
        f"RECIPE_FRAGMENT := {recipe.get('fragment', '')}",
    ]
    print("\n".join(out))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
