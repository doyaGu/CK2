# Object identity behavior baseline

Captured from CK2 commit `9473e3c3944ecc64f3f33af89030d8ea91b36ada`, before
adding native object lifetime handles. The probe uses existing SDK operations
only. The trace records IDs, high-water counts, class-list order, deletion
callbacks, PreDelete, NONOTIFY/FREEID, deferred destruction, ClearAll and a
throwing PreDelete. It contains no pointers, clocks or new lifetime values.

The three uncompressed files contain a basic object and an object-valued
parameter referring to it. Loading after 17 unrelated allocations must remap
the reference. Current output must also match the frozen file bytes. These
minimal files supplement the SDK's graph and render serialization suites;
they do not substitute for original game/editor samples.

Do not regenerate these expectations from the changed implementation.
