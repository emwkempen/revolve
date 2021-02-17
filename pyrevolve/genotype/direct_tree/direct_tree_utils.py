from collections import deque
from typing import Optional, Iterable, Tuple

from pyrevolve.revolve_bot import RevolveModule


def recursive_iterate_modules(module: RevolveModule,
                              parent: Optional[RevolveModule] = None,
                              depth: int = 1) \
        -> Iterable[Tuple[Optional[RevolveModule], Iterable, int]]:
    """
    Iterate all modules, depth search first, yielding parent, module and depth, starting from root_depth=1.
    Uses recursion.
    :param module: starting module to expand
    :param parent: for internal recursiveness, parent module. leave default
    :param depth: for internal recursiveness, depth of the module passed in. leave default
    :return: iterator for all modules with (parent,module,depth)
    """
    for _, child in module.iter_children():
        if child is not None:
            for _next in recursive_iterate_modules(child, module, depth+1):
                yield _next
    yield parent, module, depth


def subtree_size(module: RevolveModule) -> int:
    """
    Calculates the size of the subtree starting from the module
    :param module: root of the subtree
    :return: how many modules the subtree has
    """
    count = 0
    for _ in bfs_iterate_modules(root=module):
        count += 1
    return count


def bfs_iterate_modules(root: RevolveModule) \
        -> Iterable[Tuple[Optional[RevolveModule], RevolveModule]]:
    """
    Iterates throw all modules breath first, yielding parent and current module
    :param root: root tree to iterate
    :return: iterator for all modules with respective parent in the form: `(Parent,Module)`
    """
    to_process = deque([(None, root)])
    while len(to_process) > 0:
        r: (Optional[RevolveModule], RevolveModule) = to_process.popleft()
        parent, elem = r
        for _i, child in elem.iter_children():
            if child is not None:
                to_process.append((elem, child))
        yield parent, elem
