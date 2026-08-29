# COPR invokes this as:
#   make -f .copr/Makefile srpm outdir="<dir>" spec="<path>"
#
# make_srpm rather than rpkg because the Version has to be computed from git and
# written into the spec as a literal before rpmbuild parses it. rpkg evaluates
# its macros on the builder, which has no checkout and a different clock.
# tools/scripts/make-srpm.sh explains the version scheme it uses.
#
# There are no submodules here and the tarball is packed straight from the git
# index, so nothing in this step needs the network that make_srpm uniquely gets.
# The tag fetch in make-srpm.sh uses it when it is there and shrugs when it is
# not.
#
# Of the four packages below only git is genuinely missing from mock's minimal
# buildroot. The other three ship with it today; naming them costs a no-op dnf
# call rather than a broken build the day that default package set changes.
#
# The safe.directory line is not optional: COPR bind-mounts a clone made by
# another user into this chroot, and git refuses to touch a repo it thinks is
# owned by someone else.

REPO_ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))/..

.PHONY: srpm
srpm:
	dnf -y install git rpm-build tar gzip
	git config --global --add safe.directory '*'
	$(REPO_ROOT)/tools/scripts/make-srpm.sh \
	    $(if $(outdir),--outdir "$(outdir)") \
	    $(if $(spec),--spec "$(spec)")

