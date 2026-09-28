# Vulkan : transport des kernels via VK_NVX_binary_import

*English: [VULKAN.md](VULKAN.md)*

Validé sur RTX 4090 (Ada) dans No Man's Sky et DOOM: The Dark Ages (tableau en fin de page). **Pas encore validé sur RTX 30 / RTX 20**, premier cas où les images sont réécrites pour l'architecture.

## Pourquoi Vulkan demande son propre chemin

Le runtime (`nvngx_dlssg.dll`) crée ses modules CUDA par une API différente selon le moteur de rendu :

| Rendu | Création des modules | Lancement des kernels | Notre hook |
|---|---|---|---|
| D3D12 | `NvAPI_D3D12_CreateCuModule` | `NvAPI_D3D12_LaunchCuKernelChain` | `cu_module_hook.h`, `network_optimizer.h` |
| Vulkan | `vkCreateCuModuleNVX` | `vkCmdCuLaunchKernelNVX` | `vulkan_nvx.h` |

Tout ce qui se fait au chargement du runtime ne dépend pas du rendu et était déjà actif en Vulkan : barrières d'architecture, architecture minimale (`NVSDK_NGX_VULKAN_GetFeatureRequirements` compris), sélecteur de réseau Turing, recalage PTX en place, Blackwell Transfusion et politique de warp valide. Il manquait tout ce qui se fait à la création d'un module : l'abaissement sm_75 et les kernels d'image exacts (`optimizedKernels`).

## Fonctionnement

La conception suit le transport validé par dlssg_for_sm86 dans No Man's Sky (`src/companion/vulkan_transport.hpp`, adapté de RTX-Unlocker-RenoDX), sans ses tables SHA256, ses RVA fixes ni ses kernels précompilés : les images viennent de notre propre moteur, comme en D3D12.

1. **Au chargement du runtime** (`vulkan_nvx::Install`, appelé avec les autres patchs du runtime) :
   - son import `GetProcAddress` est redirigé, pour les résolutions qu'il fait lui-même (`vkGetDeviceProcAddr` depuis `vulkan-1.dll`) ;
   - son export `NVSDK_NGX_VULKAN_Init_Ext2` est détourné vers un thunk assembleur (`vulkan_nvx_thunks.asm`). Le thunk réécrit en place les arguments `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr`, puis **saute** vers l'original : le runtime voit toujours l'adresse de retour de NGX (il vérifie son appelant ; un appel la changerait).
2. **Les résolveurs enveloppés** donnent au runtime nos `vkCreateCuModuleNVX` et `vkCmdCuLaunchKernelNVX`. Tout autre module reçoit les fonctions du pilote, inchangées.
3. **La création des modules** passe par `cu_module_hook::Replacement`, le chemin D3D12 : mêmes images, même cache.
   - Un fatbin est transmis avec la taille déclarée par son en-tête : Transfusion redirige des descripteurs vers des fatbins reconstruits plus grands, alors que le runtime transmet toujours la taille d'origine, et NVX prend une taille explicite.
   - Si le pilote refuse une image réécrite, l'image du runtime est essayée (comportement d'un runtime non modifié). dlssg_for_sm86 a vu NVX refuser un PTX de blend modifié avec le pilote 616.92 ; le journal l'indique si cela se produit ici.
4. **Au premier `Init_Ext2` Vulkan**, le runtime est épinglé en mémoire, pour que le détour et l'import redirigé ne puissent jamais lui survivre. En D3D12, rien n'est épinglé et le détour n'est jamais atteint.
5. **Runtimes candidats.** NGX charge le `nvngx_dlssg` du jeu, celui du cache NGX et celui du magasin de pilotes, en garde un et décharge les autres, parfois deux fois à de nouvelles adresses (observé dans No Man's Sky). Le transport est installé sur chacun ; l'emplacement d'un runtime disparu est réutilisé aussitôt, sans attendre le scan périodique des modules.

## Lire le journal

```text
[VK-NVX] transport installed on ...\nvngx_dlssg.dll: Init_Ext2 detoured, 1 GetProcAddress import(s) redirected
[VK-NVX] NVSDK_NGX_VULKAN_Init_Ext2 reached (provider ...): no resolvers passed, the provider looks them up (redirected import), provider pinned=1
[VK-NVX] module #0 fatbin ptx86 size=... -> rewritten status=0 (accepted=1 rejected=0 rewritten=1 resized=0)
[VK-NVX] 1 provider kernel launches recorded
```

- `no resolvers passed` : le cas de No Man's Sky ; l'import redirigé achemine NVX. `resolvers wrapped` : NGX les a transmis et le thunk les a enveloppés.
- `-> rewritten` : notre image (abaissement ou kernel d'image exact) est acceptée.
- `-> size from fatbin header` : un descripteur redirigé par Transfusion.
- `-> rewritten image refused, provider image used` : NVX a refusé notre image ; à signaler.
- `status` différent de 0 : le pilote a refusé le module ; à signaler avec le journal complet.

Sans la ligne `NVSDK_NGX_VULKAN_Init_Ext2 reached`, le cœur NGX a utilisé un autre point d'entrée : seul l'import redirigé peut encore acheminer les fonctions NVX. À signaler avec le journal.

## Fonctions en Vulkan

| Fonction | Vulkan |
|---|---|
| X2–X6, raccourcis, mode `game`, plafond | Hooks Streamline, indépendants du rendu |
| Barrières d'architecture, sélecteur de réseau Turing | Patchés au chargement, par signature |
| Recalage en place, Blackwell Transfusion, politique de warp valide | Au chargement ; désormais transmis avec la bonne taille |
| Abaissement sm_75, kernels d'image exacts | Via NVX (`vulkan_nvx.h`) |
| Optimisation des réseaux DL1/DL2 et fusions de lancements | Non portées (API de lancement D3D12 uniquement) |
| Mode dynamique | Contrôleur adaptatif (ci-dessous) : multiplicateurs fixes X2–X6 choisis selon la fréquence d'images ; le Dynamic MFG de NVIDIA reste réservé à D3D12 |
| Résolution de rendu DLSS | Exports NGX D3D12 uniquement pour l'instant ; `r.ScreenPercentage` (Unreal) fonctionne |
| Overlay du multiplicateur | Dessiné par l'add-on ReShade (ci-dessous) ; l'overlay du moteur est DXGI uniquement |
| UI assist | D3D12 uniquement |

## Overlay en Vulkan

Le moteur dessine son overlay via DXGI, que les jeux Vulkan n'utilisent pas. L'add-on ReShade (`DLSSG-Transfusion.addon64`, ReShade 6.8 avec prise en charge complète des add-ons) dessine désormais le même overlay avec l'ImGui de ReShade à chaque image, menu fermé, dès que l'overlay du moteur n'a rien dessiné depuis une seconde. En D3D12, l'overlay du moteur continue de dessiner et l'add-on reste masqué : jamais deux overlays.

- Même interrupteur (`"showOverlay"`, `Ctrl + Alt + O`), même coin (`"overlayPosition"`, `Ctrl + Alt + P`) et mêmes lignes supplémentaires que l'overlay du moteur ; le texte vient du moteur (`DLSSGTransfusion_GetOverlay`).
- Première ligne `affichées/base fps Nx` : *base* est la fréquence d'images du jeu, comptée à partir de ses jetons d'image uniques de `slSetConstants` ; *affichées* est cette fréquence multipliée par le multiplicateur rapporté par DLSS-G. L'overlay DXGI compte au contraire les présentations.
- La ligne de rythme d'affichage (frame pacing) demande des présentations comptées via DXGI. Dans No Man's Sky elle apparaît : le pilote NVIDIA présente ce jeu Vulkan via une swapchain DXGI, que le moteur voit. Si un jeu Vulkan présente autrement, la ligne est absente.
- Le moteur ne dessine jamais sur cette swapchain du pilote et ne garde aucune référence à ses buffers, son device ou sa queue : il chronomètre seulement ses présentations. Y dessiner faisait planter DOOM: The Dark Ages dès que le pilote la reconstruisait (chargement d'une partie, changement de multiplicateur). Ligne du log : `Native overlay: swapchain created by nvoglv64.dll (Vulkan/OpenGL), frame pacing only; the ReShade add-on draws the overlay`.

## Mode dynamique en Vulkan : le contrôleur adaptatif

Streamline ne propose pas de Dynamic MFG natif en Vulkan. Quand `"mode": "dynamic"` est choisi (ou `Ctrl + Alt + D`) et que le runtime a été initialisé en Vulkan, le moteur continue de soumettre des multiplicateurs **fixes** et les choisit lui-même (`adaptive_policy.h`, porté de dlssg_for_sm86 où il a été validé dans No Man's Sky). D3D12 ne change pas : Dynamic MFG de NVIDIA.

- **Mesure** : un échantillon par image du jeu (jeton d'image de `slSetConstants`), filtré sur ~0,6 s. C'est la cadence des images sources, qui inclut déjà le coût de la génération d'images ; la sortie est estimée par cadence × multiplicateur, pas comptée à la présentation.
- **Décision** : après 20 échantillons et 0,75 s, et au moins 1,5 s après le changement précédent : un cran de plus si la sortie est sous 97 % de la cible, un cran de moins si un cran de moins atteint encore 99,5 % de la cible. Plafond X4, ou X6 avec `"dynamicExperimental56": true`. Cible : `"dynamicTargetFrameRate"`, ou la fréquence de l'écran si `0`.
- **Montée inutile** : si une montée n'améliore pas la sortie estimée de 1 % (jeu plafonné), elle est annulée et la montée attend 5 s, puis 10, 20 et 40 s après chaque nouvelle montée inutile ; une nouvelle cible lève l'attente.
- **Différence avec dlssg_for_sm86** : l'estimation de cadence repart de zéro après chaque changement accepté. Conservée d'un facteur à l'autre, elle portait encore ~8 % de l'ancienne cadence 1,5 s après, ce qui faisait passer une montée inutile pour un gain : un jeu plafonné grimpait jusqu'à X6.
- **Sûreté** : un changement est soumis sur le thread du jeu par le chemin de réapplication habituel. Un multiplicateur refusé par Streamline met le contrôleur en pause jusqu'au prochain changement de réglage ou à un cycle FG off/on. Un reset du jeu, un jeton d'image sauté ou un intervalle hors 1–200 ms (chargement, pause) relance la mesure, pas le multiplicateur.

L'overlay NVIDIA affiche alors le multiplicateur fixe choisi (par exemple 4x), pas une plage dynamique : c'est normal. Lignes du journal :

```text
[ADAPTIVE] Vulkan adaptive MFG at X4 (ceiling X6, target 240 FPS)
[ADAPTIVE] X4 -> X5 (source 49.3 FPS, target 240 FPS)
```

## Validation

| Test | Résultat |
|---|---|
| `tests/vulkan_nvx` : l'exécutable de test joue le runtime (exporte `Init_Ext2`, importe `GetProcAddress`) devant un faux pilote. Thunk (adresse de retour de NGX et les neuf arguments préservés, résolveurs enveloppés), image réécrite avec `pNext` conservé, repli en cas de refus, taille de l'en-tête fatbin (et son refus si la plage n'est pas lisible), comptage des lancements, redirection de l'import, transmission inchangée pour les autres modules, réinstallation après rechargement, emplacement libéré au déchargement | Tous passés (build MinGW sous Wine ; le build MSVC/MASM fait référence) |
| No Man's Sky, RTX 4090 (Ada), runtime DLSS-G du jeu (`dlfg_kernel` ancien, antérieur à 310.9.1) | Transport actif via l'import redirigé ; 128 modules créés, **0 refusé** ; le `dlfg_kernel` reconstruit par le correctif de cadence transmis avec la taille de son en-tête (98408 → 127200 octets) ; plus de 100 000 lancements du runtime ; présentation 6x puis 3x selon le jeu. Aucune image réécrite : les kernels d'image exacts demandent la 310.9.1, et Ada n'a pas besoin d'abaissement |
| Jeu Vulkan sur RTX 30 / RTX 20 | **À faire** (premier cas avec images réécrites) |
| `tests/adaptive_policy` : contrôleur face à un jeu simulé. Chaque facteur X2–X6 atteint depuis X2 et depuis X6 (sans coût : exactement ; avec coût : jamais sous le facteur suffisant le moins cher, au plus un cran au-dessus), plafond, jetons répétés, resets/sauts/pauses, jeu plafonné (pas de montée, attentes 5/10/20/40 s, nouvelle cible), facteur refusé | Tous passés |
| No Man's Sky, RTX 4090, runtime du cache NGX (`dlssg` version 20318464, kernels d'image identiques à la 310.9.1) | **Blackwell Transfusion via NVX** : 31 conteneurs recalés en sm_89, 16 descripteurs redirigés ; les trois kernels d'image exacts réécrits (BlendCandidatesFused, OutputPull, OutputPushFine) ; 64 modules, **0 refusé** |
| Même session, mode adaptatif | Cible 144 FPS (écran) : X3 à 55–60 FPS source. Cible 403 FPS : X3 → X4 → X5 → X6 à 1,5 s d'intervalle (source 58,5 → 49,0 FPS) ; plafond X4 dès que *Allow 5x and 6x* est décoché ; retour à X3 quand la cible revient à 144. Chaque changement accepté par Streamline et rapporté par DLSS-G (réel 3x/4x/5x/6x) |
| Même session, overlay dessiné par l'add-on ReShade | Affiché en haut à droite avec ses lignes supplémentaires ; première ligne `249/42 fps 6x`, `219/55 fps 4x`, `181/60 fps 3x`, cohérente avec le compteur de ReShade (≈252, 215, 180 FPS) |
| DOOM: The Dark Ages, RTX 4090, runtime 310.9.1 et Streamline 2.14.1 du jeu, 4x à ~39 FPS source en 4K | Le jeu tague des tampons HUD-less et UI (couleur et alpha) sans demander l'UI recomposition. Avec l'UIR forcé par le moteur, chaque image générée est déformée en mouvement ; avec l'UIR qui suit le jeu (`autoUiRecomposition: false`, désormais automatique pour ce jeu), les images générées sont propres. Transport : 128 modules, 0 refusé, kernels d'image exacts réécrits ; `Init_Ext2` atteint via l'import redirigé, comme dans No Man's Sky |
| DOOM: The Dark Ages, même configuration, overlay affiché | Plus de crash au chargement d'une partie ni au changement de multiplicateur : l'overlay natif ne fait que chronométrer la swapchain DXGI du pilote (`nvoglv64.dll`) et l'add-on ReShade dessine l'overlay (`118/39 fps 3x`, `UIR OFF` sans réglage manuel, ligne de frame pacing présente) |
