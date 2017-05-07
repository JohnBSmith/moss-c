
mv moss moss-tmp
mkdir -p moss
mv moss-tmp moss/moss
cp -r ../../doc moss
cp ../../home.htm moss
cp -r ../../lib moss
tar cfvj moss-linux-portable.tar.bz2 moss
# rm -r moss-portable

