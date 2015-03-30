teleinfuse est programme qui accéder d'accéder aux informations délivrées par la sortie Téléinfo des compteurs électriques.

Grâce à [FUSE](http://fuse.sourceforge.net) et un montage simple, les informations délivrées par le compteur électrique sont accessibles via un système de fichier virtuel.

![http://teleinfuse.googlecode.com/files/teleinfo-vishay-uart.png](http://teleinfuse.googlecode.com/files/teleinfo-vishay-uart.png)

Ce projet est similaire à [teleinfofs](http://code.google.com/p/teleinfofs), mais ce dernier étant écrit en Python, il est difficilement intégrable dans un périphérique embarqué (ex: routeurs, point d'accès, "box", etc).
