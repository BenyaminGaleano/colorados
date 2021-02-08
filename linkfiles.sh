#!/bin/sh

# directorio para mover las cabeceras
inclDir=/usr/local/include/

# generando enlaces a las cabeceras para threads
echo 'Enlaces para threads'
cd ./threads/
mkdir -p threads
ln -rs *.h threads
cd ..

# generando enlaces a las cabeceras para devices
echo 'Enlaces para devices'
cd ./devices/
mkdir -p devices
ln -rs *.h devices
cd ..

# generando enlaces a las cabeceras del sistema
echo 'Enlaces para lib'
cd ./lib/
sudo ln -rs *.h $inclDir
cd ..

