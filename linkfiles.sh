#!/bin/sh

# directorio para mover las cabeceras
inclDir=/usr/local/include/

# generando enlaces a las cabeceras para threads
echo -e '\n\e[32mEnlaces para threads\e[0m\n'
ln -rs threads threads/
ln -rs threads devices/
ln -rs threads lib/kernel/
ln -rs threads tests/threads/
ln -rs threads userprog/
ln -rs devices threads/


# generando enlaces a las cabeceras para devices
echo -e '\n\e[32mEnlaces para devices\e[0m\n'
echo ''
ln -rs devices devices/

# generando enlaces a las cabeceras del sistema
echo -e '\n\e[32mEnlaces para lib\e[0m\n'
cd ./lib/
sudo ln -rs debug.h $inclDir
sudo ln -rs random.h $inclDir
sudo ln -rs round.h $inclDir
cd ..

# generando enlaces a cabeceras necesarios para kernel o user
echo -e '\n\e[32mEnlaces para lib/kernel\e[0m\n'
cd ./lib/kernel/
sudo ln -rs list.h $inclDir
sudo ln -rs console.h $inclDir
cd ../..


echo -e '\n\e[32mEnlaces para lib/user/\e[0m\n'
cd ./lib/user/

cd ../..

# generando enlaces a las cabeceras para test/threads
echo -e '\n\e[32mEnlaces para devices\e[0m\n'
echo ''
ln -rs tests tests/threads


