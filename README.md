*teleinfuse* est programme qui permet d'accéder aux informations délivrées par la sortie *Téléinfo* des compteurs électriques.

Grâce à [FUSE](http://fuse.sourceforge.net) et un montage simple, les informations délivrées par le compteur électrique sont accessibles via un système de fichier virtuel.

![teleinfo-vishay-uart.png](https://github.com/neomilium/teleinfuse/blob/master/img/teleinfo-vishay-uart.png)

Ce projet est similaire à [teleinfofs](http://code.google.com/p/teleinfofs), mais ce dernier étant écrit en Python, il est difficilement intégrable dans un périphérique embarqué à faible ressource (ex: routeurs, point d'accès, "box", etc).
