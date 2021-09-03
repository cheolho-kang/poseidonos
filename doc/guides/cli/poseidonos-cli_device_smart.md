## poseidonos-cli device smart

Show SMART information from an NVMe log page.

### Synopsis

Show SMART information from an NVMe log page.

Syntax:
	poseidonos-cli device smart (--device-name | -d) DeviceName .
          

```
poseidonos-cli device smart [flags]
```

### Options

```
  -d, --device-name string   The name of device to get the SMART information
  -h, --help                 help for smart
```

### Options inherited from parent commands

```
      --debug         Print response for debug
      --fs string     Field separator for the output (default "|")
      --ip string     Set IPv4 address to PoseidonOS for this command (default "127.0.0.1")
      --json-req      Print request in JSON form
      --json-res      Print response in JSON form
      --port string   Set the port number to PoseidonOS for this command (default "18716")
      --unit          Display unit (B, KB, MB, ...) when displaying capacity
```

### SEE ALSO

* [poseidonos-cli device](poseidonos-cli_device.md)	 - Device commands for PoseidonOS.
