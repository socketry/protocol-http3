# Protocol::HTTP3

Provides a low-level HTTP/3 protocol implementation using [ngtcp2](https://github.com/ngtcp2/ngtcp2) and [nghttp3](https://github.com/ngtcp2/nghttp3).

[![Development Status](https://github.com/socketry/protocol-http3/workflows/Test/badge.svg)](https://github.com/socketry/protocol-http3/actions?workflow=Test)

## Features

  - Native HTTP/3 server and dispatcher primitives.
  - Integration with `protocol-quic` sockets, configuration and TLS contexts.
  - Extensible Ruby subclasses for servers and dispatchers.

## Usage

Please see the [project documentation](https://socketry.github.io/protocol-http3/) for more details.

## Releases

Please see the [project releases](https://socketry.github.io/protocol-http3/releases/index) for all releases.

### v0.0.1

## Contributing

We welcome contributions to this project.

1.  Fork it.
2.  Create your feature branch (`git checkout -b my-new-feature`).
3.  Commit your changes (`git commit -am 'Add some feature'`).
4.  Push to the branch (`git push origin my-new-feature`).
5.  Create new Pull Request.

### Running Tests

To run the test suite:

``` shell
bundle exec sus
```

### Making Releases

To make a new release:

``` shell
bundle exec bake gem:release:patch # or minor or major
```

### Developer Certificate of Origin

In order to protect users of this project, we require all contributors to comply with the [Developer Certificate of Origin](https://developercertificate.org/). This ensures that all contributions are properly licensed and attributed.

### Community Guidelines

This project is best served by a collaborative and respectful environment. Treat each other professionally, respect differing viewpoints, and engage constructively. Harassment, discrimination, or harmful behavior is not tolerated. Communicate clearly, listen actively, and support one another. If any issues arise, please inform the project maintainers.
