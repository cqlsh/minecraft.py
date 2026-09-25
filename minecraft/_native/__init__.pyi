"""
The C++ core behind minecraft, with one submodule per accelerated Python
module. Plugins never import it; the Python modules on top of it are the
supported API.
"""