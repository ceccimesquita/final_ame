# HAR TinyML — Raspberry Pi Pico

Reconhecimento de atividade humana (HAR) embarcado em tempo real usando uma CNN 1D quantizada (INT8) rodando diretamente na Raspberry Pi Pico com TensorFlow Lite Micro. O sensor MPU6050 fornece acelerômetro e giroscópio a 50 Hz; o modelo classifica janelas de 2,56 s em quatro atividades: **WALKING**, **SITTING**, **STANDING** e **LAYING**.

---

## Estrutura do projeto

```
.
├── src/
│   ├── main.cpp          # pipeline de aquisição, pré-processamento e inferência
│   └── mpu6050.cpp       # driver MPU6050 para Pico SDK
├── include/
│   └── mpu6050.h
├── model/
│   └── cnn_raw_har_model.h   # modelo convertido para array C (TFLite flatbuffer)
├── cnn_4classes_HAR.ipynb    # notebook de treinamento e conversão do modelo
└── CMakeLists.txt
```

---

## Dependências

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [pico-tflmicro](https://github.com/raspberrypi/pico-tflmicro) — TensorFlow Lite Micro portado para Pico
- Python 3.10+ com os pacotes do notebook (ver seção abaixo)

---

## Instalação

### 1. Pico SDK e pico-tflmicro

```bash
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk && git submodule update --init
export PICO_SDK_PATH=/caminho/para/pico-sdk

git clone https://github.com/raspberrypi/pico-tflmicro.git
```

Edite o `CMakeLists.txt` para apontar `add_subdirectory` ao caminho local do `pico-tflmicro`.

### 2. Dependências Python

```bash
pip install numpy pandas scikit-learn tensorflow matplotlib
```

---

## Como rodar o notebook

O notebook `cnn_4classes_HAR.ipynb` realiza o treinamento da CNN no dataset UCI-HAR e exporta o modelo quantizado para `model/cnn_raw_har_model.h`.

```bash
jupyter notebook cnn_4classes_HAR.ipynb
```

Execute todas as células em ordem. Ao final, o arquivo `model/cnn_raw_har_model.h` será gerado automaticamente.

---

## Como compilar e gravar na Pico

### Compilar

```bash
mkdir build && cd build
cmake ..
make -j4
```

O arquivo `.uf2` será gerado em `build/mpu6050_pico.uf2`.

### Gravar

1. Segure o botão **BOOTSEL** da Pico e conecte o cabo USB ao computador.
2. A Pico aparecerá como um drive USB (`RPI-RP2`).
3. Copie o arquivo `.uf2` para o drive:

```bash
cp build/mpu6050_pico.uf2 /media/$USER/RPI-RP2/
```

A Pico reinicia automaticamente e começa a classificar as atividades. A saída pode ser monitorada via USB serial (115200 baud):

```bash
minicom -b 115200 -o -D /dev/ttyACM0
```

---

## Links

- Artigo: _em breve_
- Slides: _em breve_
