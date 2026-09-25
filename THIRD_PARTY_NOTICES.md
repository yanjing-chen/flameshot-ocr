# Third-party notices

This repository is a derivative work based on Flameshot and integrates an OCR
client with a separately installed Local AI Runtime.

## Flameshot

Upstream: https://github.com/flameshot-org/flameshot

The original Flameshot source and this derivative remain subject to the
repository's existing license terms. Keep the upstream `LICENSE` file with
redistributions.

## Local AI Runtime

Project: https://github.com/yanjing-chen/local-ai-runtime

Local AI Runtime is installed and maintained independently for the current
user. It is not bundled into Flameshot OCR packages and is not removed when
Flameshot OCR is uninstalled.

Local AI Runtime manages its own llama.cpp runtimes, model files and hardware
backends. Review that project's notices and the licenses of installed runtime
and model components separately.

## PaddleOCR-VL-1.6 GGUF

Project/model publisher: PaddlePaddle

Model page:
https://huggingface.co/PaddlePaddle/PaddleOCR-VL-1.6-GGUF

The model is not stored in this repository or bundled in the generated
AppImage/deb. Local AI Runtime downloads and manages it separately.

License shown by the model publisher: Apache License 2.0.

## OCR icon

Icon: OCR
Designer: Pictogrammers Team
Icon pack: Material Design Icons
Source:
https://www.iconarchive.com/show/material-icons-by-pictogrammers/ocr-icon.html

License: Apache License 2.0.

The repository also contains:
`data/img/material/OCR_ICON_LICENSE.txt`

## No endorsement

Names and trademarks of upstream projects are used only to identify the
software components on which this fork depends. This fork is not an official
release of Flameshot, PaddlePaddle, llama.cpp, or Local AI Runtime.
