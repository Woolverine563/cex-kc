git submodule update --init
git submodule update --recursive --remote
pushd .
cd dependencies/abc
git apply ../abc.patch
make libabc.a -j4
popd
