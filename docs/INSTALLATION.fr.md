# Installation

*[English](INSTALLATION.md) · [中文](INSTALLATION.zh-CN.md)*

## Ce qu'il faut

- Windows 10 ou 11, 64 bits.
- Une GeForce RTX 20, RTX 30 ou RTX 40 avec un pilote NVIDIA récent.
- Pour la génération d'images : un jeu qui propose **DLSS Frame Generation** dans ses réglages.
- Pour Smooth Motion et Neural Rendering, voir leurs pages :
  [Smooth Motion](SMOOTH-MOTION.fr.md), [Neural Rendering](NEURAL-RENDERING.fr.md).

> [!WARNING]
> RTX Encore se charge dans le jeu. Ne l'installez pas dans un jeu en ligne ou protégé par un anti-triche : il peut
> être bloqué, ou faire signaler un compte.

## Contenu de l'archive

| Fichier | Rôle |
| :--- | :--- |
| `rtx-encore.dll` | Le mod. À copier à côté du jeu, sous le nom que le jeu charge (ci-dessous). |
| `alternative-proxies\rtx-encore.asi` | Le même mod sous forme de plugin, pour les jeux qui ont un chargeur ASI. |
| `alternative-proxies\version.dll`, `winmm.dll`, `dxgi.dll`, `dinput8.dll` | Copies déjà nommées, pour les rares jeux qui n'acceptent pas le fichier principal renommé. |
| `cyberpunk-2077-cet-panel\` | Panneau optionnel pour Cyber Engine Tweaks (voir [Cyberpunk 2077](#cyberpunk-2077)). |
| `README.md`, `docs\`, `THIRD-PARTY-NOTICES.md` | Cette documentation et les mentions des composants tiers. |

Installez **une seule** copie du mod par jeu, jamais deux.

## Installer

1. Trouvez le dossier de l'exécutable qui fait réellement tourner le jeu. Pour un jeu Unreal Engine, c'est
   `<Jeu>\Binaries\Win64\`, à côté de `<Jeu>-Win64-Shipping.exe`, et non le petit lanceur à la racine.
2. Copiez-y `rtx-encore.dll` et renommez-le. Essayez les noms dans cet ordre :

   | Nom | Quand |
   | :--- | :--- |
   | `version.dll` | Premier choix. Fonctionne dans la plupart des jeux, dont Unreal Engine 4 et 5. |
   | `dinput8.dll` | Quand `version.dll` n'est pas chargé ou est déjà pris par un autre mod. |
   | `winmm.dll` | Idem, autre voie. |
   | `dxgi.dll` | Jeux qui ne chargent que celui-ci. Ne remplacez pas un `dxgi.dll` qui appartient à ReShade, Special K ou DXVK. |

   Le fichier fonctionne aussi sous les noms `d3d9.dll`, `d3d10.dll`, `d3d11.dll`, `d3d12.dll`, `dsound.dll`,
   `wininet.dll`, `winhttp.dll`, `binkw64.dll`, `bink2w64.dll`, `xinput1_1.dll`, `xinput1_2.dll`, `xinput1_3.dll`,
   `xinput1_4.dll`, `xinput9_1_0.dll` et `xinputuap.dll`. Le nom décide seulement de la façon dont le jeu charge le
   mod ; il n'ajoute la prise en charge d'aucune autre API graphique. Avec un nom Bink, gardez à côté le fichier
   d'origine du jeu, renommé `binkw64Hooked.dll` ou `bink2w64Hooked.dll`.
3. Si un autre mod utilise déjà un nom, laissez-lui son fichier et choisissez un autre nom pour RTX Encore.
4. Lancez le jeu. Au premier lancement, le menu s'ouvre une fois ; **Inser** le rouvre.
5. Activez **DLSS Frame Generation** dans les réglages du jeu. S'il l'était déjà, désactivez-le puis réactivez-le,
   ou relancez le jeu.

### Comme plugin ASI

Si le jeu a un chargeur ASI (Ultimate ASI Loader, celui fourni avec Cyber Engine Tweaks, ...), copiez
`alternative-proxies\rtx-encore.asi` dans le dossier que ce chargeur lit, en général `plugins` ou `scripts`, au
lieu de renommer la DLL.

### Ce qui apparaît à côté du mod

- `rtx-encore.jsonc` : vos réglages, créés au premier lancement. Voir [Fichier de réglages](SETTINGS.fr.md).
- `rtx-encore-logs\` : les journaux de session, les trois plus récents par défaut.

## Mettre à jour

Remplacez le fichier que le jeu charge par le nouveau `rtx-encore.dll`, **sous le nom qu'il porte déjà**
(`version.dll`, `dinput8.dll`, ...), ou remplacez `rtx-encore.asi`. Votre `rtx-encore.jsonc` est conservé : ne le
supprimez pas.

## Mise à niveau depuis un ancien nom

RTX Encore a d'abord été publié sous le nom DLSSG-Transfusion (versions jusqu'à `v1.4.5.3-rtx20-30-40`), et des
versions de développement se sont appelées RTX Unlocker.

- Remplacez l'ancien fichier sous le nom qu'il porte dans le dossier du jeu, ou supprimez-le et installez le
  nouveau. Ne laissez jamais deux copies.
- Vos réglages sont conservés : `DLSSG-Transfusion.json`, `RTX-Unlocker.jsonc` ou `RTX-Unlocker.json` est renommé
  `rtx-encore.jsonc` au premier lancement, avec ses valeurs.
- Le plugin a changé de nom. Si `DLSSG-Transfusion.asi` est resté à côté de `rtx-encore.asi`, le mod le remarque :
  l'ancien fichier tourne une dernière fois, est renommé `DLSSG-Transfusion.asi.replaced`, et seul `rtx-encore.asi`
  se charge à partir du lancement suivant. Le fichier `.replaced` peut être supprimé.
- L'add-on ReShade des anciennes versions (`DLSSG-Transfusion.addon64`) ne sert plus : le menu est intégré.
  Retirez-le du dossier du jeu.
- Les anciens journaux laissés à côté du mod sont déplacés dans `rtx-encore-logs`.

## Désinstaller

Supprimez le fichier que vous avez ajouté (`version.dll` ou le nom choisi, ou `rtx-encore.asi`). Vous pouvez aussi
supprimer `rtx-encore.jsonc` et le dossier `rtx-encore-logs`. Le mod ne modifie jamais un fichier du jeu ou du
pilote sur le disque.

## Notes par jeu

### Unreal Engine 4 et 5

Placez le fichier à côté de `<Jeu>-Win64-Shipping.exe`, sous `<Jeu>\Binaries\Win64\`, puis activez DLSS et Frame
Generation dans le jeu.

Certains jeux Unreal Engine embarquent DLSS sans le proposer dans leurs menus. Il s'active souvent depuis le
fichier `Engine.ini` du jeu :

- Unreal Engine 5 : `%LOCALAPPDATA%\<Projet>\Saved\Config\Windows\Engine.ini`
- Unreal Engine 4 : `%LOCALAPPDATA%\<Projet>\Saved\Config\WindowsNoEditor\Engine.ini`

`<Projet>` est le nom du projet dans le moteur, qui peut différer du titre du jeu. Lancez le jeu une fois pour que
le fichier existe, fermez-le, faites une sauvegarde, puis ajoutez :

```ini
[SystemSettings]
r.NGX.Enable=1
r.NGX.DLSS.Enable=1
r.TemporalAA.Upsampling=1
r.ScreenPercentage=67

[/Script/DLSS.DLSSSettings]
bEnableDLSSD3D12=True
```

`r.ScreenPercentage` est la résolution de rendu : 100 pour DLAA, 67 Qualité, 58 Équilibré, 50 Performance, 33 Ultra
Performance. Si le jeu réécrit `Engine.ini` au démarrage, passez le fichier en lecture seule après l'avoir modifié.

### Cyberpunk 2077

Utilisez soit `bin\x64\version.dll`, soit `bin\x64\plugins\rtx-encore.asi` avec le chargeur ASI fourni avec Cyber
Engine Tweaks. Laissez à vos autres mods les fichiers dont ils ont besoin, sous leur nom : `version.dll` pour Cyber
Engine Tweaks, `winmm.dll` pour RED4ext, `dxgi.dll` pour ReShade.

Le menu (**Inser**) fonctionne seul. Si vous utilisez Cyber Engine Tweaks, un panneau optionnel montre aussi les
commandes principales dans son overlay : copiez le dossier `bin` qui se trouve dans `cyberpunk-2077-cet-panel` dans
le dossier du jeu. Ce panneau est expérimental et demande que le plugin `.asi` soit installé.

### DXVK, ReShade, Special K, OptiScaler

- Laissez `dxgi.dll` à l'outil qui le fournit et donnez un autre nom à RTX Encore.
- Avec DXVK, le menu s'attache à la présentation Vulkan.
- Avec OptiScaler, voir l'option de flip metering dans
  [Génération d'images](FRAME-GENERATION.fr.md#options-de-compatibilité).
