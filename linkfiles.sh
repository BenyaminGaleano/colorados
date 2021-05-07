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
ln -rs threads vm/
ln -rs threads filesys/
ln -rs devices threads/

# generando enlaces de userprog
echo -e '\n\e[32mEnlaces para userprog\e[0m\n'
ln -rs userprog userprog/
ln -rs userprog vm/
ln -rs userprog filesys/

# generando enlaces de userprog
echo -e '\n\e[32mEnlaces para vm\e[0m\n'
ln -rs vm threads/
ln -rs vm userprog/
ln -rs vm vm/
ln -rs vm filesys/

# generando enlaces de filesys
echo -e '\n\e[32mEnlaces para filesys\e[0m\n'
ln -rs filesys userprog/
ln -rs filesys filesys/
ln -rs filesys vm/

# generando enlaces a las cabeceras para devices
echo -e '\n\e[32mEnlaces para devices\e[0m\n'
ln -rs devices devices/
ln -rs devices filesys/

# generando enlaces a las cabeceras del sistema
echo -e '\n\e[32mEnlaces para lib\e[0m\n'
cd ./lib/
sudo ln -rs debug.h $inclDir
sudo ln -rs random.h $inclDir
sudo ln -rs round.h $inclDir
sudo ln -rs syscall-nr.h $inclDir
cd ..

# generando enlaces a cabeceras necesarios para kernel o user
echo -e '\n\e[32mEnlaces para lib/kernel\e[0m\n'
cd ./lib/kernel/
sudo ln -rs list.h $inclDir
sudo ln -rs hash.h $inclDir
sudo ln -rs console.h $inclDir
sudo ln -rs bitmap.h $inclDir
cd ../..


echo -e '\n\e[32mEnlaces para lib/user/\e[0m\n'
cd ./lib/user/

cd ../..

# generando enlaces a las cabeceras para test/threads
echo -e '\n\e[32mEnlaces para devices\e[0m\n'
echo ''
ln -rs tests tests/threads



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
sudo ln -rs bitmap.h $inclDir
cd ../..


echo -e '\n\e[32mEnlaces para lib/user/\e[0m\n'
cd ./lib/user/

cd ../..

# generando enlaces a las cabeceras para test/threads
echo -e '\n\e[32mEnlaces para devices\e[0m\n'
echo ''
ln -rs tests tests/threads


