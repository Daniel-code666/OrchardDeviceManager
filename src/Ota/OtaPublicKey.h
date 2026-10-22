#pragma once

// Clave publica de desarrollo. La clave privada correspondiente permanece en
// .ota-local/ y nunca debe incluirse en el firmware ni en el repositorio.
constexpr char OTA_SIGNING_PUBLIC_KEY[] = R"PEM(-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEfgElx+vtcEcfRuUIjPaFkxMlNXeN
MMMyEz+emF/Uk7SkX04+WRyvJaJOktOAWAQPy8Bg+YKN8OQ+8v0LagVSLA==
-----END PUBLIC KEY-----
)PEM";

