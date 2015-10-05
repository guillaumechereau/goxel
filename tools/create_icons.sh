./goxel --icons imgs/icons.gox.gz
mogrify -resize 32x32 data/icons/*.png
pngquant --ext .png --force data/icons/*.png
