The libcgroup Release Process
===============================================================================
https://github.com/libcgroup/libcgroup

This is the process that should be followed when creating a new libcgroup
release.

#### 1. Update the libcgroup-tests submodule

	# ./bootstrap.sh
	# git submodule update  --remote tests
	# git add tests
	# git commit -s
	# git push

#### 2. Verify that all issues assigned to the release milestone have been resolved

  * https://github.com/libcgroup/libcgroup/milestones

#### 3. Verify that the Github Actions are all passing

#### 4. Verify that the bundled test suite runs without error

	# ./bootstrap.sh
	# make check

#### 5. Verify that the packaging is correct

	# make distcheck

#### 6. Perform any distribution test builds

  * Oracle Linux
  * Fedora Rawhide
  * Red Hat Enterprise Linux
  * etc.

#### 7. If any problems were found up to this point that resulted in code changes, restart the process

#### 8. If this is a new major/minor release, create new 'release-X.Y' branch

	# git branch "release-X.Y"

#### 9. Update the version number in configure.ac AC_INIT(...) macro

#### 10. Tag the release in the local repository with a signed tag

	# git tag -s -m "version X.Y.Z" vX.Y.Z

#### 11. Build final release tarball

	# make clean
	# ./bootstrap.sh
	# make dist-gzip

#### 12. Verify the release tarball in a separate directory

	<unpack the release tarball in a temporary directory>
	# ./configure --sysconfdir=/etc --localstatedir=/var \
	--enable-opaque-hierarchy="name=systemd" --enable-python
	# make check

#### 13. Generate a checksum for the release tarball

	# sha256sum <tarball> > libcgroup-X.Y.Z.tar.gz.SHA256SUM

#### 14. GPG sign the release tarball and checksum using the maintainer's key

	# gpg --armor --detach-sign libcgroup-X.Y.Z.tar.gz
	# gpg --clearsign libcgroup-X.Y.Z.tar.gz.SHA256SUM

#### 15. Push the release tag to the main GitHub repository

	# git push <repo> vX.Y.Z

#### 16. Create a new GitHub release using the associated tag and upload the following files

  * libcgroup-X.Y.Z.tar.gz
  * libcgroup-X.Y.Z.tar.gz.asc
  * libcgroup-X.Y.Z.tar.gz.SHA256SUM
  * libcgroup-X.Y.Z.tar.gz.SHA256SUM.asc

#### 17. Update the GitHub release notes for older releases which are now unsupported

The following Markdown text is suggested at the top of the release note, see old GitHub releases for examples.

```
***This release is no longer supported upstream, please use a more recent release***
```
