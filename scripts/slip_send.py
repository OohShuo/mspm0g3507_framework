#!/usr/bin/env python3
"""
SLIP 协议串口发送脚本。

将一篇文章通过串口以 SLIP 协议分包发送：
- 帧头: 0x7F, 帧尾: 0x7E, 转义符: 0x7D
- 转义: 0x7F→0x7D 0x02 | 0x7E→0x7D 0x01 | 0x7D→0x7D 0x00
- 每块最多 250 字节转义后数据（板子 RX 512 字节足够）
- 10 Hz 发送

用法:
    python3 slip_send.py <串口> [--baud 115200] [--chunk-size 250] [--hz 10]
"""

import argparse
import codecs
import os
import serial
import sys
import textwrap
import time

# ── Protocol constants ──────────────────────────────────────────────
SLIP_START     = 0x7F
SLIP_END       = 0x7E
SLIP_ESC       = 0x7D
SLIP_ESC_START = 0x02
SLIP_ESC_END   = 0x01
SLIP_ESC_ESC   = 0x00

# ── Article (~8 KB) ───────────────────────────────────────────────
ARTICLE_TXT = textwrap.dedent("""\
    Artificial Intelligence: From Turing to Transformers
    ====================================================

    1. The Dawn of AI

    The dream of creating intelligent machines predates the computer age.
    In 1950, Alan Turing published "Computing Machinery and Intelligence,"
    proposing the famous Turing Test as a criterion for machine intelligence.
    Six years later, John McCarthy organized the Dartmouth Summer Research
    Project on Artificial Intelligence, where the term was officially coined.
    The participants -- including Marvin Minsky, Herbert Simon, and Allen
    Newell -- were optimistic that significant progress could be made within
    a single generation.

    Early AI systems focused on symbolic reasoning. The Logic Theorist,
    developed by Newell and Simon in 1956, could prove mathematical theorems
    by manipulating symbols according to formal rules. This approach viewed
    intelligence as the manipulation of abstract representations, much like
    human reasoning with language and logic. Expert systems, which encoded
    domain knowledge as if-then rules, achieved commercial success in the
    1970s and 1980s in fields like medical diagnosis (MYCIN) and chemical
    analysis (DENDRAL). These systems could outperform human experts within
    their narrow domains but proved brittle and difficult to maintain.

    2. The AI Winters

    Despite early enthusiasm, AI research faced significant setbacks. The
    first "AI winter" began in the mid-1970s when the limitations of
    symbolic approaches became apparent. Systems that worked well in
    laboratory settings failed to scale to real-world complexity. The
    Lighthill Report in the UK led to funding cuts, and DARPA reduced its
    AI investment in the United States. A second winter followed in the
    late 1980s after the Lisp machine market collapsed and Japan's Fifth
    Generation project failed to deliver on its ambitious goals. Neural
    network research, which had a revival through backpropagation, again
    faced skepticism as training deep networks proved impractical with
    the hardware of the era.

    3. The Deep Learning Revolution

    The landscape shifted dramatically around 2012. Three factors converged:
    massive labeled datasets like ImageNet, powerful GPU computing hardware,
    and algorithmic innovations that made training deep networks practical.
    AlexNet, a convolutional neural network by Krizhevsky, Sutskever, and
    Hinton, achieved a stunning victory in the 2012 ImageNet challenge with
    an error rate nearly half that of the second-best competitor. This
    watershed moment convinced the computer vision community that deep
    learning was not just theoretically interesting but practically superior.

    In the following years, deep learning achieved breakthroughs across
    domains. Recurrent neural networks and their variants (LSTM, GRU)
    became standard for sequence tasks like machine translation and speech
    recognition. Generative adversarial networks (GANs), introduced by Ian
    Goodfellow in 2014, opened new frontiers in image generation. Deep
    reinforcement learning enabled DeepMind's DQN to master Atari games
    from raw pixels and AlphaGo to defeat world champion Lee Sedol in 2016
    through self-play and Monte Carlo tree search.

    Computer vision evolved rapidly. ResNet introduced skip connections
    that enabled training networks with over 100 layers. Object detection
    systems like YOLO and Faster R-CNN achieved real-time performance.
    Semantic segmentation networks could label every pixel in an image,
    enabling applications from autonomous driving to medical imaging.

    4. The Transformer Era

    In 2017, "Attention Is All You Need" by Vaswani et al. introduced the
    Transformer architecture, fundamentally reshaping AI. Unlike recurrent
    networks that process tokens sequentially, Transformers use self-
    attention to compute relationships between all pairs of tokens in
    parallel. This enables efficient training on massive corpora and
    captures long-range dependencies that RNNs struggle with.

    The key innovation is multi-head self-attention. For each token, the
    model computes query, key, and value vectors. Attention weights between
    tokens are determined by scaled dot products, normalized via softmax.
    Multiple heads allow the model to attend to different aspects of the
    input simultaneously -- one head might track syntactic relationships
    while another captures semantic meaning. Positional encodings provide
    token order information since attention is permutation-invariant.

    5. GPT and Large Language Models

    OpenAI's GPT series demonstrated the power of scaling Transformers.
    GPT-1 (2018) showed that pre-training a Transformer decoder on a large
    corpus followed by task-specific fine-tuning achieved strong results.
    GPT-2 (2019) scaled to 1.5 billion parameters and exhibited zero-shot
    capabilities, causing OpenAI to delay its release over misuse concerns.
    GPT-3 (2020) reached 175 billion parameters, performing novel tasks
    through in-context learning without any gradient updates.

    ChatGPT launched in November 2022, combining GPT-3.5 with instruction
    tuning and Reinforcement Learning from Human Feedback (RLHF). It could
    engage in natural conversation, answer follow-up questions, admit
    mistakes, and reject inappropriate requests. Within two months it
    reached 100 million users -- the fastest-growing consumer app ever.
    GPT-4 followed in March 2023 with multimodal capabilities, accepting
    both text and images, and achieving human-level scores on the Uniform
    Bar Exam and Medical Licensing Examination.

    Other major players joined the race. Google launched Bard (later Gemini),
    Anthropic released Claude with a focus on safety through Constitutional
    AI, and Meta open-sourced LLaMA, spurring an explosion of community
    fine-tuned variants. By 2024, frontier models could reason through
    complex multi-step problems, write and debug code, and process hundreds
    of pages of context in a single interaction.

    6. Multimodal and Generative AI

    AI expanded beyond text into multiple modalities. CLIP by OpenAI
    connected text with images by training on 400 million image-text pairs,
    enabling zero-shot classification. DALL-E, Stable Diffusion, and
    Midjourney generated coherent images from text prompts. Video generation
    followed with Sora creating minute-long videos with coherent motion.

    Whisper achieved human-level speech recognition across dozens of
    languages. Music models like Suno composed complete songs from text.
    Code generation tools like GitHub Copilot became indispensable, with
    AI-written code approaching 50% of new code by 2025.

    7. Edge AI and TinyML

    A parallel revolution unfolded at the resource-constrained extreme.
    TinyML deploys machine learning on microcontrollers with as little as
    a few kilobytes of RAM. Model compression via weight pruning removes
    unimportant connections. Quantization reduces precision from 32-bit
    floating point to 8-bit integers (or even 4-bit), dramatically shrinking
    model size and accelerating inference on hardware without FPUs. Knowledge
    distillation trains a small "student" model to mimic a larger "teacher."

    Applications include smart speakers with on-device keyword spotting,
    industrial accelerometers performing vibration analysis for predictive
    maintenance, and medical wearables monitoring heart rhythms for atrial
    fibrillation -- all running on-device to preserve privacy and eliminate
    cloud round-trip latency.

    8. Challenges and the Road Ahead

    As AI grows more capable, critical challenges demand attention. AI
    alignment -- ensuring systems pursue goals consistent with human values
    -- remains an open research problem with growing urgency. Fairness and
    bias in AI can amplify societal inequalities when training data reflects
    historical discrimination, with real-world impact on hiring, lending,
    criminal justice, and healthcare decisions.

    Interpretability is crucial. Deep learning models are "black boxes"
    whose internal representations resist interpretation. Mechanistic
    interpretability aims to reverse-engineer neural network computations.
    Training large models has substantial environmental cost -- GPT-3
    produced over 500 tons of CO2 -- making energy efficiency and renewable
    power increasingly important.

    Despite these challenges, AI's potential benefits are immense. From
    accelerating scientific discovery (like AlphaFold predicting protein
    structures) to enabling personalized education, from improving climate
    models to discovering new materials and drugs, AI could help address
    humanity's most pressing problems. The key is developing these
    technologies thoughtfully, with robust governance frameworks and broad
    societal participation in decisions about deployment and regulation.
    """)

# Encode to bytes (ASCII-only, utf-8 is a no-op here)
ARTICLE_BYTES = ARTICLE_TXT.encode("utf-8")
assert 7500 <= len(ARTICLE_BYTES) <= 9000, \
    f"Article is {len(ARTICLE_BYTES)} bytes, expected ~8K"


def slip_escape(data: bytes) -> bytes:
    """Escape data according to SLIP rules."""
    out = bytearray()
    for b in data:
        if b == SLIP_START:
            out.extend((SLIP_ESC, SLIP_ESC_START))
        elif b == SLIP_END:
            out.extend((SLIP_ESC, SLIP_ESC_END))
        elif b == SLIP_ESC:
            out.extend((SLIP_ESC, SLIP_ESC_ESC))
        else:
            out.append(b)
    return bytes(out)


def slip_unescape(data: bytes) -> bytes:
    """Unescape SLIP-encoded data (for verification only)."""
    out = bytearray()
    esc = False
    for b in data:
        if esc:
            if b == SLIP_ESC_START:
                out.append(SLIP_START)
            elif b == SLIP_ESC_END:
                out.append(SLIP_END)
            elif b == SLIP_ESC_ESC:
                out.append(SLIP_ESC)
            else:
                out.append(b)
            esc = False
        elif b == SLIP_ESC:
            esc = True
        else:
            out.append(b)
    return bytes(out)


def send_frame(ser: serial.Serial, payload: bytes,
               chunk_size: int = 250, hz: float = 10.0) -> None:
    """
    Send a SLIP-framed payload over serial.

    Splits escaped bytes into chunks.  First chunk gets START prepended,
    last chunk gets END appended.  Chunks are sent at `hz` Hz.
    """
    escaped = slip_escape(payload)

    chunks = [
        escaped[i : i + chunk_size]
        for i in range(0, len(escaped), chunk_size)
    ]

    interval_s = 1.0 / hz
    total = len(escaped)
    print(f"Article: {len(payload)} B → escaped: {total} B → "
          f"{len(chunks)} chunk(s) @ {hz} Hz")

    for idx, chunk in enumerate(chunks):
        if idx == 0:
            chunk = bytes((SLIP_START,)) + chunk
        if idx == len(chunks) - 1:
            chunk = chunk + bytes((SLIP_END,))

        ser.write(chunk)
        n = ser.out_waiting  # pending bytes in OS TX buffer
        if hasattr(ser, "flush"):
            ser.flush()
        print(f"  [{idx + 1}/{len(chunks)}] {len(chunk)} B sent"
              f"{'' if n == 0 else f' ({n} B pending)'}")

        if idx < len(chunks) - 1:
            time.sleep(interval_s)

    # Round-trip verification (decode locally)
    roundtrip = slip_unescape(escaped)
    ok = "OK" if roundtrip == payload else "MISMATCH"
    print(f"Round-trip verify: {ok}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send article over serial with SLIP framing")
    parser.add_argument("port", help="Serial port, e.g. /dev/ttyACM0 or COM3")
    parser.add_argument("--baud", type=int, default=115200,
                        help="Baud rate (default: 115200)")
    parser.add_argument("--chunk-size", type=int, default=250,
                        help="Max escaped bytes per chunk (default: 250)")
    parser.add_argument("--hz", type=float, default=10.0,
                        help="Send rate in Hz (default: 10)")
    args = parser.parse_args()

    print(f"Opening {args.port} @ {args.baud} baud ...")
    ser = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(0.1)  # let DTR/RTS settle

    send_frame(ser, ARTICLE_BYTES, chunk_size=args.chunk_size, hz=args.hz)

    ser.close()
    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
