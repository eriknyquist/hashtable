A lightweight separate-chaining, arena-backed hashtable in C
------------------------------------------------------------

See `hashtable API documentation <https://eriknyquist.github.io/hashtable/>`_

Run tests
---------

Requires GNU make, and a version of GCC that supports Address Sanitizer and UB Sanitizer (any reasonably
recent version of GCC).

::

    cd unit_tests
    make

Generate performance visualization
----------------------------------

Requires python 3.x and the python ``matplotlib`` package.

::

    cd perf_tests
    make
