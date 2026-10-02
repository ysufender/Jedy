rm -rf .git/
cd ..
git clone https://codeberg.org/ysufender/Jedy.git JEDY_COPY
mv JEDY_COPY/.git Jedy/
rm -rf JEDY_COPY
cd Jedy
git stat

