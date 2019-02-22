#! /bin/sh

set +x
# Prefer aclocal 1.9 if we can find it
(which aclocal-1.11 &> /dev/null && aclocal-1.11 -I m4) ||
(which aclocal-1.9 &>/dev/null && aclocal-1.9 -I m4) ||
	aclocal  -I m4

# Darwin bizarrely uses glibtoolize
(which libtoolize &>/dev/null && libtoolize --force --copy) ||
	glibtoolize --force --copy

(which autoheader2.50 &>/dev/null && autoheader2.50) || 
autoheader

# Prefer automake-1.9 if we can find it
(which automake-1.11 &>/dev/null && automake-1.11 --add-missing --copy --foreign) ||
(which automake-1.10 &>/dev/null && automake-1.10 --add-missing --copy --foreign) ||
(which automake-1.9  &>/dev/null && automake-1.9  --add-missing --copy --foreign) ||
	automake --add-missing --copy --foreign

(which autoconf2.50 &>/dev/null && autoconf2.50) || 
autoconf
