# FieldKilroy userspace bridge (canonical)

Synced from `AMOURANTHRTX/FieldKilroy` — layout **v9**, **micro** units, ioctl via `/dev/kilroy_field`.

Build (static smoke):

```bash
g++ -O2 -I../../include/uapi -c FieldKilroyKernel.cpp -o FieldKilroyKernel.o
```

Proc writes (`/proc/kilroy_field/thermo`, `fcc`) remain the portable path; ioctl pulls CACHE/DIRECT slots.