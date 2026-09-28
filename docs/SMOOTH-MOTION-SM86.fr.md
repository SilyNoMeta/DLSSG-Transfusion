# Smooth Motion du pilote sur RTX 30 (SM86)

*English: [SMOOTH-MOTION-SM86.md](SMOOTH-MOTION-SM86.md)*

État au 28 septembre 2026 : **analyse statique reproductible, aucune activation livrée**.

Cette étude concerne le `NvPresent64.dll` du pilote NVIDIA 617.14, pas le runtime
`nvngx_dlssg.dll` déjà pris en charge par Transfusion. Les deux chemins font de la
génération d'images, mais leurs ABI, leur chargement et leurs kernels sont distincts.

## Référence exacte

| Élément | Valeur |
|---|---|
| Fichier | `NvPresent64.dll` |
| Pilote | `32.0.16.1714` / chaîne interne `DVSReal r617_07 617.14` |
| DriverStore | `nvami.inf_amd64_b259952749ea1a39` |
| Taille | 8 525 544 octets |
| SHA256 | `1716d13d169320a2dc71486733202ed75127e8f39cd679a9193848fc1fa45e7f` |
| GPU cible | RTX 3070 Ti Laptop, Ampere SM86 |

Le binaire NVIDIA n'est pas copié dans ce dépôt. Le relevé généré, sans code
propriétaire, est dans
[`evidence/nvpresent-617.14.json`](evidence/nvpresent-617.14.json). Pour le
reproduire :

```powershell
py -3 scripts\analyze_nvpresent.py `
  "C:\Windows\System32\DriverStore\FileRepository\nvami.inf_amd64_b259952749ea1a39\NvPresent64.dll" `
  --expect-sha256 1716d13d169320a2dc71486733202ed75127e8f39cd679a9193848fc1fa45e7f
```

Le script est en lecture seule, n'importe ni ne charge la DLL et n'en produit
aucune copie modifiée.

## Résultat kernels : Ampere oui, Turing non

La DLL contient 37 fatbins. Chacun contient exactement un ELF `sm_89` et un ELF
`sm_120`; le nombre d'entrées PTX est **zéro**.

| Famille | Kernels SM89 | Retarget SM86 | Motif |
|---|---:|---|---|
| FP16 | 20 | **possible sur ce build** | SASS décodée à l'identique en SM86; seul le nom affiché de `F2FP...PACK_AB` change, pas l'encodage |
| FP8 (`*_fp8`) | 17 | **interdit** | `QMMA.16832.F16.E4M3.E4M3` est illégal en SM86; les conversions E4M3 seraient mal décodées |
| Total | 37 | 20 seulement | aucun PTX de repli |

Les 20 kernels FP16 sont `conv1..8`, `conv_fused`, `conv_proj1/2`,
`conv_out1..3`, `attn1/2`, `depth_to_space`, `downscale_kernel`,
`warp_coarse_kernel` et `main_kernel`. Les registres (maximum 255) et la mémoire
partagée (maximum 16 Kio) restent dans les limites de SM86.

Les 37 ELF SM89 ont ici le même `e_flags`, `0x06005904`. Un retarget propre doit
remplacer **uniquement** le champ architecture (`0x59` vers `0x56`) et préserver
les autres bits. Écrire `0x06005604` comme une constante n'est acceptable que
pour cette empreinte exacte, pas comme algorithme général.

Sur Turing, le simple retarget est exclu : il n'existe aucun PTX à recompiler et
la SASS FP16 emploie notamment `HMMA.16816`, `LDGSTS` et `REDUX`, indisponibles
avant SM80. Cette conclusion porte sur le chemin NvPresent 617.14; elle ne dit
pas qu'un remplacement complet du modèle et de son exécuteur serait
théoriquement impossible, mais ce ne serait plus un portage par compatibilité de
métadonnées.

## Initialisation D3D réellement effectuée

L'export `NVP_Init_D3D` est à `+0x59e0`. Son flot est court et peut être résumé
ainsi :

```text
NVP_Init_D3D
  -> évalue config, masque API D3D = 3             (+0x9fd0)
  -> exige effective_enabled                      (+0x9ed0)
  -> si DX11 (+0x4f) ou DX12 (+0x50) est permis
       -> charge dxgi.dll / CreateDXGIFactory2     (+0x37960)
       -> détoure CreateSwapChain et CreateSwapChainForHwnd
  -> sinon réussit sans installer le chemin D3D
```

`+0x37960` charge `dxgi.dll`, résout `CreateDXGIFactory2`, crée une factory et
travaille sur les slots de vtable `+0x50` et `+0x78`. Ils correspondent aux
slots 10 (`CreateSwapChain`) et 15 (`CreateSwapChainForHwnd`) d'`IDXGIFactory2`.
Ce n'est pas l'initialiseur CUDA.

### Structure de configuration à `+0x7f0cd0`

Les rôles suivants sont établis par les accès et les chaînes de log du binaire :

| Offset | Rôle établi |
|---:|---|
| `+0x48` | masque testé contre le masque API demandé (`3` pour D3D) |
| `+0x4c` | override du veto produit par ce masque; ce n'est pas le booléen Smooth Motion principal |
| `+0x4e` | autorisation Vulkan |
| `+0x4f` | autorisation DX11 |
| `+0x50` | autorisation DX12 |
| `+0xe8` | précondition lue par la création du support device; sens public non encore nommé |
| `+0xe9` | `Smooth Motion enabled` (nom confirmé par le log interne) |
| `+0x12a1` | précondition d'initialisation calculée |
| `+0x12a3` | précondition fixée à vrai dans ce chemin |
| `+0x12a4` | résultat du masque/override et de `+0xe9` |
| `+0x12a5` | activation effective mise en cache |

La formule à `+0xa05b` est :

```text
config[0x12a4] =
  ((config[0x48] & requested_api_mask) != 0 && config[0x4c] == 0)
    ? 0
    : config[0xe9]

config[0x12a5] =
  config[0x12a1] && config[0x12a3] && config[0x12a4]
```

Cela explique pourquoi écrire `+0x4c=1` et `+0xe9=1` active le build étudié,
mais ne justifie pas de les traiter comme deux booléens interchangeables. Toute
implémentation doit capturer les valeurs, appliquer les modifications comme une
transaction et restaurer en cas d'échec.

## Gate device et choix FP16/FP8

Le gate commence à `+0xc400`. Son test déterminant est :

```asm
+0xc41c  cmp   dword ptr [rcx+14h], 3
...
+0xc42f  cmovge ebp, ebx
...
+0xc437  setge sil
```

Le getter minimal à `+0xbd10` retourne ce même champ `[rcx+0x14]`. Le sélecteur
de précision à `+0x7ff90` compare l'architecture reçue à `3` à `+0x7ffce`.
En dessous de 3 il garde le chemin FP16; le chemin FP8 n'est admissible qu'à
partir d'Ada. Sur une vraie RTX 30 le champ reste donc `2`.

Le patch minimal Ampere est de changer **l'immédiat du seuil** à `+0xc41f`, de
`3` vers `2`, et de laisser `SETGE SIL` intact. Remplacer aussi `SETGE SIL` par
`MOV SIL,1`, comme le fait la révision NVSmooth30 étudiée, rend la décision vraie
même sur une architecture inférieure au nouveau seuil et détruit la barrière
Turing. Ce second patch est inutile sur Ampere et ne doit pas être repris.

Le gate n'est appelé qu'après les préconditions globales (`config+0xe8` et
activation effective) et calcule encore plusieurs dépendances. L'activation ne
doit donc pas être réduite à « un octet d'architecture ».

## Resolver CUDA à `+0x1348d0`

`+0x1348d0` est un wrapper `InitOnceExecuteOnce`, pas une fonction sans contexte
dont le contrat serait inconnu. Son callback :

1. charge `nvcuda.dll` avec `LoadLibraryExA(..., 0x800)`;
2. cherche `cuGetProcAddress_v2`, avec repli `cuGetProcAddress`;
3. remplit les groupes du dispatch CUDA;
4. renvoie `0` en cas de succès et `-1` en cas d'échec via le wrapper.

Le slot observé à `+0x7fb628` est rempli après ce resolver dans le build de
référence. Le probe natif révèle que sa cible exécutable est dans
`nvcuda64.dll` du **même paquet DriverStore**, tandis que `nvcuda.dll` expose
un relais. Le prototype accepte uniquement ce chemin de paquet ou le module
`nvcuda.dll` lui-même. Vérifier seulement que le slot est non nul ne suffit pas.

## Revue de NVSmooth30 actuel

Révision relue :
[`ItsAdeline/NVSmooth30@21fa435`](https://github.com/ItsAdeline/NVSmooth30/commit/21fa43521c2753ffd5c8e9d2092cfc79f9c6c456),
datée du 21 septembre 2026. Son inspecteur reconnaît bien le build 617.14 de ce
document, même si son fixture versionné cite une autre empreinte SHA256 ayant le
même fingerprint structurel.

Les améliorations par rapport à la première revue sont réelles : scan du gate
plus contraint, transaction/rollback, plusieurs chemins d'interception CUDA,
partage NT-handle avec repli, query D3D11 bornée, tentative de détection tardive
du wrapper et réinitialisation du bridge quand la forme de swapchain change.

Les points bloquants restants sont :

- les 37 cubins SM89 sont toujours retargetés, y compris les 17 FP8; ils sont
  dormants sur Ampere tant que le sélecteur reste correct, mais ne constituent
  pas une frontière de sécurité;
- `e_flags` est toujours remplacé par une constante;
- le gate remplace aussi `SETGE SIL` par vrai, ce qui retire la barrière Turing;
- le fallback 617.14 appelle encore les RVA privés `+0x1348d0` et `+0x7fb628`;
- `+0x4c` et `+0xe9` sont réécrits, mais seul le second a maintenant un rôle
  explicitement identifié;
- les slots privés de vtable 19/20 restent appelés sur un objet trouvé derrière
  la swapchain;
- la DLL DriverStore est encore choisie par date de modification; un module
  NvPresent déjà chargé n'est pas adopté en priorité;
- le bridge crée d'abord le device D3D12 avec `nullptr`, malgré la LUID de
  l'adaptateur du jeu déjà disponible;
- le jeu ne présente plus sa swapchain quand le bridge réussit; la shadow
  swapchain présente à sa place sur le même HWND;
- il subsiste une attente CPU de la query D3D11 et une attente de fence D3D12
  par image;
- espace colorimétrique/HDR, métadonnées HDR et rotation ne sont pas propagés;
  `ResizeBuffers` n'est pas intercepté directement;
- le bootstrap reste asynchrone et peut arriver après la factory du jeu.

Le code amont est donc une source d'hypothèses et de séquences observées, pas un
composant à fusionner tel quel.

## Architecture proposée pour Transfusion

1. **Limiter la première étape à D3D12 natif.** Valider le chargement, le gate,
   les 20 modules FP16 et l'attachement NvPresent sans introduire le bridge
   D3D11. Examiner ensuite le chemin D3D11 natif avant tout bridge.
2. **Identifier le module actif.** Adopter d'abord un `NvPresent64.dll` déjà
   chargé; sinon résoudre le paquet du pilote associé à l'adaptateur NVIDIA du
   jeu. Ne jamais choisir « le fichier le plus récent ».
3. **Allowlist de build structurelle.** Exiger export, formule de config, gate
   unique, getter, sélecteur et resolver cohérents. La SHA exacte reste une
   preuve, pas le seul mécanisme de versionnage.
4. **Patch transactionnel minimal.** Seuil `3 -> 2`, valeurs config capturées,
   hook CUDA installé avant l'init, rollback complet au moindre échec. Ne pas
   forcer `SETGE`.
5. **Retarget sélectif.** Parser le fatbin et l'ELF, identifier le kernel, ne
   modifier que les 20 noms FP16 allowlistés, préserver tous les bits de
   `e_flags` hors architecture et laisser SM120/FP8 inchangés.
6. **Échec fermé.** Si un module FP8 est demandé sur SM86, si le sélecteur ne
   correspond plus, ou si une structure est ambiguë, désactiver Smooth Motion
   plutôt que soumettre du code machine douteux au pilote.
7. **Pas de Turing.** Refuser explicitement SM75/SM70 avant tout patch.
8. **Diagnostics probants.** Journaliser SHA, chemin actif, LUID, anciennes et
   nouvelles valeurs, noms des modules retargetés, compte FP8 laissé intact,
   retours CUDA et rollback.

## Validation encore requise pour une compatibilité générale

- contrôle offline du build avec `scripts/analyze_nvpresent.py`;
- test unitaire du parser sur copies synthétiques tronquées/corrompues;
- preuve que seuls 20 cubins FP16 changent et que SM120/FP8 restent identiques;
- validation D3D12 réelle sur RTX 3070 Ti Laptop : création, 300+ images,
  resize, alt-tab, plein écran/fenêtré, arrêt propre;
- logs prouvant que le champ architecture reste `2` et que le chemin FP8 n'est
  jamais sélectionné;
- capture identique avant/après pour vérifier cadence, frametimes et 1% lows;
  le fait que les kernels s'exécutent ne prouve ni qualité ni gain;
- jeux HDR et SDR séparés; D3D11 natif à tester séparément, bridge seulement
  si le chemin natif échoue;
- aucun test dans un jeu compétitif ou protégé par anti-cheat.

## Inconnues encore ouvertes

- sens public exact de `config+0xe8`, `config+0x48` et provenance de
  `config+0x4c`;
- contrat des méthodes privées de wrapper 19/20 et stabilité inter-pilotes;
- mécanisme supporté pour attacher NvPresent à une swapchain D3D12 déjà créée;
- comportement de plusieurs swapchains/plusieurs HWND;
- coût, qualité et stabilité en jeu du modèle FP16 sur Ampere;
- compatibilité d'autres branches de pilote.

## Prototype opt-in dans Transfusion

Le prototype est dans `source/native/smooth_motion_sm86.cpp` et s'active par
`"smoothMotionSm86": true` dans la section `compatibility` du fichier
`DLSSG-Transfusion.json`, suivi d'un redémarrage du jeu. Sa valeur par défaut
est `false`. `"smoothMotionSm86Api": "d3d12"` conserve le chemin validé;
`"d3d11"` et `"vulkan"` sont des expériences distinctes, observées chacune
dans un jeu mais pas validées pour une compatibilité générale.
Il cible une vraie RTX 30; la détection de famille GPU doit rester sur `auto`.

Le prototype charge `NvPresent64.dll` déjà présent en mémoire, ou le chemin
DriverStore exact `nvami.inf_amd64_b259952749ea1a39` du système analysé. Il
exige SHA256 `1716d13d169320a2dc71486733202ed75127e8f39cd679a9193848fc1fa45e7f`
et les signatures de l'export, du gate, du getter, du sélecteur FP16/FP8 et du
resolver. Il installe un intercepteur de `cuModuleLoadData` sur le slot privé,
vérifie que l'original réside dans `nvcuda.dll` ou le `nvcuda64.dll` du même
paquet, puis abaisse seulement le
seuil du gate `3 -> 2`. Le mode par défaut désactive Vulkan et D3D11 dans
sa configuration interne. Les 20 fatbins FP16 sont modifiés en mémoire,
un à un, en préservant le reste du conteneur. Aucun fichier du pilote n'est
modifié sur disque.
Une fois le hook installé, le module du proxy reste chargé jusqu'à la fin du
processus pour que le slot CUDA ne pointe jamais vers du code déchargé.

Le test `smooth_motion_fatbin_test` exécuté sur le DLL de référence trouve
20 FP16 retargetés et 17 FP8 refusés, avec exactement deux octets modifiés
par FP16. Cela valide le parseur et les métadonnées, pas l'exécution des
kernels ou le rendu. Le probe `smooth_motion_sm86_probe` a aussi validé le
chargement, le resolver et le retour d'activation de `NVP_Init_D3D` dans un
processus isolé sur cette machine. Un prototype alternatif consistant à convertir les FP8
en FP16 demanderait de reconstruire la SASS et les formats de poids, de
chargement et de conversion de chaque kernel; changer `QMMA` en `HMMA` ou
renommer l'architecture ne suffit pas. Le chemin FP16 déjà embarqué rend
cette conversion inutile pour Ampere.

Validation en jeu rapportée par l'utilisateur sur Manor Lords (D3D12, RTX 3070 Ti
Laptop) : génération affichée « MFG 2X » par FrameView, 120 images/s avec un
plafond de 60 images/s dans le jeu et la génération native désactivée. Une
première comparaison avec les deux contrôles désactivés revenait à 60 images/s.
Un essai supplémentaire avec le profil NVIDIA Smooth Motion sur Off et
`smoothMotionSm86` sur `true` active encore la génération : pour ce pilote et
ce jeu, la clé JSON suffit et ne se contente pas d'accompagner le profil.
Ces observations valident le fonctionnement visible dans ce cas, mais pas
encore les coûts, la qualité image par image, ni la stabilité prolongée.

Limites actuelles : le paquet DriverStore est fixé à ce portable; le module
actif n'est pas relié à un LUID d'adaptateur; l'installation dans le worker
peut arriver après la création de la factory DXGI du jeu; l'échec tardif de
`NVP_Init_D3D` ne permet pas de retirer avec certitude tous ses éventuels hooks
DXGI. Aucun rendu HDR ni autre jeu n'a encore été validé. Ne pas activer par
défaut avant les tests de stabilité et de qualité listés ci-dessus.

La conclusion reste donc : **Ampere est techniquement atteignable sur 617.14 par
le chemin FP16, Turing ne l'est pas par retarget, et l'intégration production
reste à mesurer et durcir.**

## Étude de faisabilité D3D11 et Vulkan après validation D3D12

Le succès D3D12 ne démontre pas les autres API. Les mêmes 20 kernels FP16
peuvent vraisemblablement servir aux trois backends, mais la capture de la
swapchain, la synchronisation et l'initialisation sont propres à chaque API.
L'analyse suivante concerne toujours uniquement le build 617.14.

### D3D11 : essayer le backend NvPresent natif avant un bridge

`NvPresent64.dll` exporte `NVP_CreateSwapchain_D3D11` à `+0x5990`, distinct de
`NVP_CreateSwapchain_D3D12` à `+0x59a0`. Le premier délègue à `+0x9470`, le
second à `+0x9600`; leurs chemins vérifient respectivement un type interne `1`
et `0`, puis construisent des objets distincts (`+0x44fb0` contre `+0x45200`).
Le module importe aussi `D3D11CreateDevice`. `NVP_Init_D3D` teste
`config+0x4f` (D3D11) et `config+0x50` (D3D12), et installe le hook DXGI si
l'un des deux est autorisé. Le mode D3D12 force `+0x4f=0`; le nouveau mode
`d3d11` met `+0x4f=1` et `+0x50=0`, puis appelle `NVP_Init_D3D`. Le probe
isolé confirme son retour d'activation, pas encore une présentation réelle.

Conclusion : **D3D11 fonctionne sans bridge D3D11→D3D12 dans Shadows of
Doubt**, d'après le test en jeu du 28 septembre 2026 : cap 60 FPS, FrameView
120 FPS. Le journal confirme l'initialisation du backend natif et 19 kernels
FP16 acceptés par CUDA (`status=0`), sans exception dans le journal de
diagnostic. Cela ne valide pas encore tous les jeux, formats et modes de
swapchain. L'expérience à élargir est un mode D3D11 isolé qui conserve les mêmes
contrôles SHA/gate/CUDA, met `+0x4f=1` et `+0x50=0` avant l'init, puis journalise
la capture d'une swapchain D3D11 réelle, ses Present, les retours CUDA et la
cadence externe. Tester ensuite flip/blit, resize, alt-tab, HDR, waitable
swapchain, multi-adaptateur et coexistence ReShade. Une image doublée et une
sortie propre doivent être vérifiées avant de proposer un mode utilisateur.
Si ce backend échoue, le bridge de NVSmooth30 reste une solution distincte,
mais ses deux swapchains pour un HWND, sa synchronisation CPU/GPU, son choix
d'adaptateur et la propagation HDR exigent une refonte, pas un simple port.

### Vulkan : backend et couche présents, activation encore inconnue

Le DLL exporte `NVP_Init_Vulkan` à `+0x5a50`. Cette fonction demande le masque
API `4` (contre `3` pour D3D), vérifie l'activation effective et passe trois
arguments au chemin d'initialisation `+0x61890`. Le mode D3D12 force
`config+0x4e=0`. Le nouveau mode `vulkan` met `+0x4e=1`, mais **n'appelle pas**
`NVP_Init_Vulkan` : les trois arguments privés doivent venir de la couche
NVIDIA. Il journalise seulement que le backend est armé.
Dans le même paquet DriverStore, `nv-vk64.json` décrit la couche
`VK_LAYER_NV_present` fournie par `nvoglv64.dll`; `vulkaninfo --summary` la
liste effectivement sur cette machine. Cela établit sa présence, pas son
chargement ni son fonctionnement dans un jeu Vulkan lorsque le profil NVIDIA
est Off.

Conclusion : **Vulkan paraît faisable sans wrapper D3D**, via la couche NVIDIA
existante, sous réserve de comprendre qui appelle `NVP_Init_Vulkan`, avec
quels arguments, et à quel moment la couche est chargée. Le prochain test
doit observer la liste des couches *actives* et les appels d'initialisation
sur un processus Vulkan avec le mode expérimental, lequel installe le patch
SM86/FP16 avant `vkCreateInstance`. Le simple fait de mettre le bit à `1`
ou d'armer le hook ne prouve pas la génération; appeler l'export à l'aveugle
serait dangereux.
Contrôler la sélection de l'adaptateur sur portable hybride, les files de
présentation, la synchronisation, le resize et l'ordre des couches Steam,
ReShade et OBS. Sur Enshrouded, l'utilisateur observe la génération seulement
avec Smooth Motion activé dans le profil NVIDIA; `--keep-vulkan-layers` est
inutile dans ses deux essais. Le journal du succès montre la swapchain de
`nvoglv64.dll` et 19 kernels FP16 acceptés par CUDA (`status=0`).

### Pourquoi le profil est encore nécessaire en Vulkan

La différence avec D3D11 est maintenant visible dans les binaires de ce
paquet. Notre mode D3D11 appelle directement `NVP_Init_D3D`; le mode Vulkan
ne peut pas appeler `NVP_Init_Vulkan` sans les trois arguments de la couche.
Dans `nvoglv64.dll` (48 639 720 octets, SHA256
`68b2d0f82e69e6bb7af67ea554c11caba783e2c75f5ab475db108ff55bf1313c`),
le bloc à `+0xda33a4` interroge l'identifiant de profil `0xB0D384C0`
(Smooth Motion - Enable). À `+0xda33b9`, la valeur zéro saute le chargement
de NvPresent. Un réglage intermédiaire non identifié (`0xB09B15AF`) et un
état global du pilote peuvent aussi interrompre le chemin à `+0xda33e3`.
Le même bloc interroge `0xB0CC0875` (Enabled APIs) à `+0xda3403`; à
`+0xda341a`, il exige le bit `4` de Vulkan. Ce n'est qu'après ces contrôles
que le code résout `NVP_Init_Vulkan` et l'appelle à
`+0xda3471` avec les arguments du pilote. Dans `NvPresent64.dll`, ces mêmes
identifiants sont lus à `+0x26dbb` et `+0x26f4f` pour remplir respectivement
les autorisations d'API et l'état d'activation que le prototype modifie.

Ainsi, **forcer seulement les champs NvPresent ne contourne pas la décision
antérieure de `nvoglv64.dll`**. Un forçage sans modifier le profil est plausible
sur ce build en faisant passer les contrôles de ce bloc dans le module Vulkan,
uniquement en mémoire et après vérification exacte du DLL. Mais aucun patch
de `nvoglv64.dll` n'a été appliqué ni testé. Avant de l'implémenter, relever
les valeurs effectives des deux réglages et le retour de `NVP_Init_Vulkan`
avec profil Off/On; refuser tout autre build et éviter un appel manuel de
l'export avec des pointeurs inventés. L'autre voie est de régler explicitement
le profil via NVAPI avant le lancement, ce qui change durablement le profil
du jeu et n'est pas un vrai contournement.
Forcer la découverte de `VK_LAYER_NV_present` via le loader Vulkan ne suffit
pas à contourner cette vérification interne de profil.

### Diagnostic du profil Vulkan (lecture seule)

La cible optionnelle `smooth_motion_drs_probe` interroge, avec le SDK NVAPI
officiel, les trois identifiants ci-dessus dans le profil de l'exécutable et
dans le profil global. Elle ne modifie ni le profil ni les DLL du jeu. Fournir
`-DNVAPI_INCLUDE_DIR=<dossier contenant nvapi.h>` à CMake, compiler la cible,
puis lancer `smooth_motion_drs_probe.exe` avec le chemin absolu de
`enshrouded.exe`. Répéter une fois profil Smooth Motion Off et une fois On,
jeu fermé. Conserver les deux sorties : `GetSetting status=-160` signifie
« réglage absent de ce profil », **pas** « valeur zéro effective ». Le
diagnostic ne prouve pas à lui seul que `NVP_Init_Vulkan` a été appelé ; le
retour de cette fonction reste une instrumentation distincte à réaliser.

Observation du 28 septembre 2026 : après activation explicite du profil
Enshrouded par l'utilisateur, ces trois IDs renvoient encore `-160` dans le
profil de l'application **et** le profil global. Un réglage témoin connu
(`0x1034CB89`, FXAA) est lisible avec la même fonction : la sonde NVAPI
fonctionne, mais ces IDs ne sont pas exposés par `NvAPI_DRS_GetSetting` dans
cette configuration. Ce résultat ne contredit pas le succès Vulkan observé
en jeu et ne donne pas la valeur effective lue par `nvoglv64.dll`. Il faut
désormais observer le chemin privé à l'exécution avant d'en déduire un
contournement sûr ; ne pas interpréter `-160` comme Off.

Un observateur expérimental est désormais compilé dans le proxy seulement
pour `smoothMotionSm86Api=vulkan` et après validation SHA/signatures du
`NvPresent64.dll` 617.14. Il intercepte **sans modifier les arguments**
`NVP_Init_Vulkan`, appelle l'original, puis journalise « call observed » et
son retour booléen. Cela ne contourne pas le gate de `nvoglv64.dll` et ne
force pas Smooth Motion. Son absence dans le journal peut signifier que le
gate a bloqué l'appel, mais aussi que la couche n'a pas été chargée ; vérifier
la swapchain et FrameView en parallèle.

Validation Enshrouded du 28 septembre 2026 avec **la même DLL** (SHA256
`CEBCD4918CA14C6AD4856C993268A52CECB54CFCC59E28BFF6949F25D8E25CFF`)
et la même configuration JSON :

- Profil NVIDIA On : quatre appels observés à `NVP_Init_Vulkan`, chacun avec
  retour `1`; swapchain `nvoglv64.dll`; 19 kernels FP16 acceptés par CUDA
  (`status=0`).
- Profil NVIDIA Off : trace installée, mais aucun appel à `NVP_Init_Vulkan`,
  aucune swapchain `nvoglv64.dll` observée par notre overlay et aucun kernel
  Smooth Motion chargé pendant ce lancement.

Le contraste établit que le profil intervient **avant** l'initialisation de
NvPresent en Vulkan. Il ne suffit pas à isoler lequel des trois contrôles
privés change, puisque `NvAPI_DRS_GetSetting` ne les expose pas. Un prototype
de forçage doit donc être limité à ce build, vérifier les octets attendus
dans `nvoglv64.dll` et refuser tout autre pilote. Le retour `1` et les chargements CUDA ne
mesurent pas, à eux seuls, le nombre d'images générées ; utiliser FrameView
pour cette validation distincte.

### Prototype de forçage Vulkan pour le pilote 617.14

Quand `smoothMotionSm86=true` et `smoothMotionSm86Api=vulkan`, le proxy tente
maintenant, au **premier retour de chargement** de `nvoglv64.dll`, de
neutraliser uniquement le saut qui écarte le chemin Smooth Motion lorsque
`0xB0D384C0` vaut zéro. Les contrôles suivants (`0xB09B15AF`, autorisation
Vulkan `0xB0CC0875`) restent actifs. Le patch ne modifie que six octets en
mémoire dans le processus du jeu ; il ne change ni fichier du pilote ni
profil NVIDIA.

Le refus est la règle en cas de doute : `NvPresent64.dll` doit déjà avoir
passé sa validation SHA/signatures, `nvoglv64.dll` doit venir du même dossier
DriverStore, sa taille et son SHA256 doivent correspondre au build 617.14
inspecté, et les instructions aux trois contrôles doivent correspondre
exactement. Si le chargement initial est manqué, le proxy ne patche pas un
module déjà utilisé. Un nouveau pilote demandera donc une nouvelle analyse
et une mise à jour explicite. Un `try/catch` n'offre pas de protection contre
un branchement machine erroné ou un plantage GPU ; il ne remplace pas ces
vérifications.

Le probe autonome a confirmé dans un processus de test que le module étranger
est refusé, que le module exact est accepté et que seuls les six octets visés
sont neutralisés.

**Validation Enshrouded, profil NVIDIA Off, 28 septembre 2026 :** l'utilisateur
confirme que Smooth Motion fonctionne avec le nouveau proxy (SHA256
`C9B49CB35FA80042CE9CBA3E0FF57B30AE7E4B091BDEB960552DA46B4824EA01`).
Le journal montre le patch appliqué à `nvoglv64.dll`, quatre appels à
`NVP_Init_Vulkan` retournant `1`, la swapchain de la couche NVIDIA et 19
kernels FP16 chargés avec `status=0`. Le journal d'exceptions ne contient
aucun événement. La valeur FrameView de ce lancement n'a pas été relevée
dans cette conversation ; le ressenti de l'utilisateur et les traces
d'initialisation ne remplacent pas une mesure FPS avant/après chiffrée.

**No Man's Sky, Vulkan, 28 septembre 2026 — déconseillé :** son profil
NVIDIA définit Enabled APIs (`0xB0CC0875`) à `3`, sans le bit Vulkan `4`.
À cette valeur, notre patch du build vérifié s'applique, mais aucun appel à
`NVP_Init_Vulkan` ni chargement des kernels Smooth Motion ne suit. Avec la
valeur temporaire `4` ou `7`, l'utilisateur confirme que Smooth Motion
fonctionne, mais le curseur se triple lorsqu'il bouge la souris. Ce n'est
donc **pas** une validation de compatibilité visuelle. Le retour à `3`
bloque de nouveau Smooth Motion. Le profil montre une exclusion propre à ce
jeu ; nous ne connaissons ni la raison du choix de NVIDIA ni la cause exacte
de l'artefact. Ne pas recommander le forçage du profil pour jouer
normalement. Cette observation est distincte du chemin Vulkan DLSS-G.

Après le retour à `3`, l'utilisateur signale aussi la disparition des
options DLSS et DLSS-G dans le jeu. Le journal actuel charge toujours leurs
modules ; les réglages sauvegardés indiquent `AntiAliasing=None` et
`DLSSFrameGeneration=Off`. Cela ne prouve pas que l'expérience de profil en
est la cause. Tester d'abord `smoothMotionSm86=false`, puis sans le proxy si
nécessaire, avant de relier ce symptôme à Smooth Motion.

Conserver une copie de la DLL précédente. Sur un pilote différent, le patch
doit être refusé ; après toute mise à jour du pilote, tester de nouveau dans
un jeu sans anti-cheat et vérifier le journal avant d'utiliser la fonction.

Référence : [API DRS officielle NVIDIA](https://docs.nvidia.com/nvapi/group__drsapi.html).

### Déploiement expérimental recommandé

- D3D11 : Shadows of Doubt, `dxgi.dll` à côté de `Shadows of Doubt.exe`,
  `smoothMotionSm86=true`, `smoothMotionSm86Api=d3d11`. Le répertoire examiné
  ne contenait pas de proxy préexistant. Vérifier d'abord que le journal montre
  « D3D11 backend initialized », puis comparer FrameView Off/On à scène et cap
  identiques; un retour d'init seul ne suffit pas.
- Vulkan : Enshrouded, `dinput8.dll` à côté de `enshrouded.exe` (cet import est
  présent dans l'exécutable), `smoothMotionSm86=true`,
  `smoothMotionSm86Api=vulkan`. Avec le build 617.14 vérifié et le nouveau
  proxy, le profil NVIDIA peut rester Off ; `--keep-vulkan-layers` n'est pas
  requis. Le journal « Vulkan backend armed » ne prouve **pas** que la couche
  NvPresent s'est chargée : vérifier le message de patch, les appels
  `NVP_Init_Vulkan`, puis FrameView. Le déploiement doit rester réversible.

Ne pas empiler Smooth Motion avec la génération d'images native, ne pas tester
avec anti-cheat, et retirer le proxy si le jeu devient instable. Les deux
modes sont isolés derrière une clé API explicite; `d3d12` reste le défaut.

Références publiques : [prise en charge D3D11/D3D12/Vulkan annoncée par
NVIDIA](https://www.nvidia.com/en-eu/geforce/news/nvidia-app-global-dlss-overrides-rtx-40-series-smooth-motion/)
et [description de la couche Vulkan Smooth Motion sous
Linux](https://download.nvidia.com/XFree86/Linux-x86_64/575.57.08/README/nvpresent.html).
La seconde décrit un autre système et ne prouve pas le mécanisme Windows.
