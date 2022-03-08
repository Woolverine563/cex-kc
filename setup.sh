git submodule update --init
pushd .
cd dependencies/abc
git apply ../abc.patch
make libabc.a -j4
popd