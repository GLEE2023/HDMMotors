# STM32 protocol v2 migration

The UI calls semantic methods from `MotorProtocol`, not Arduino pin assignments. An STM32 USB CDC device can present the same COM-port transport; implement `Stm32Protocol` with the same commands and status model. Recommended v2 additions: version negotiation and capabilities, request IDs, ACK/ERROR responses, sequence-step confirmation IDs, started/progress/completed/cancelled events, sensor feedback and interlock flags, explicit stop acknowledgment, and an independent hardware E-stop. Do not assume an ACK proves physical motion or launch.
