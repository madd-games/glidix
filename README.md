# Glidix

## Install the required packages (Ubuntu)

To install all required packages, run:

```
sudo apt install g++ nasm
```

## Set up cross-compiler `PATH`

We recommend adding this to `~/.profile` to add the cross-compiler binary directory to the `PATH`:

```
export PATH="$PATH:$HOME/glidix/cross/bin"
```

## Building the cross-compiler (stage 1)

To build the first stage of the cross-compiler, which allows us to then build the dependencies of the second stage,
create a build directory outside of the repository root, for example named `glidix-cross`. Thus the directory structure
should be as follows:

```
/glidix
  /.git
  /kernel
  ...
/glidix-cross
  [cd here]
```

Inside the `glidix-cross` directory we run the following commands:

```
../glidix/setup-crosstools-workspace --sysroot=$HOME/glidix        # Downloads and patches the build tools source code.
./install.sh                                                       # Builds and installs the cross-compiler.
```

## Configuring the Glidix ISO build

Now that we have the first stage of the cross-compiler built and installed, we can configure the build to create an
installation ISO. Create a build directory next to the root, called for example `glidix-build`, such that your directory
structure is now like this:

```
/glidix
  /.git
  /kernel
  ...
/glidix-cross
  /install.sh
  ...
/glidix-build
  [cd here]
```

Inside the `glidix-build` directory, run:

```
../glidix/configure --sysroot=$HOME/glidix --iso
```

## Build and install the C library in the sysroot

Inside `glidix-build`, run:

```
make libc                                   # Build the C library
DESTDIR=$HOME/glidix make install-libc      # Install the C library in the sysroot
```

## Building the cross-compiler (stage 2)

Now that the Glidix C library is installed in the sysroot, we can build the second stage of the compiler,
which depends on it. Back in `glidix-cross`, run:

```
sudo ./install-stage2.sh                    # Install the second stage
```

## Installing other libraries in the sysroot

Back in `glidix-build`:

```
make libz
DESTDIR=$HOME/glidix make install-libz
make libpng
DESTDIR=$HOME/glidix make install-libpng
make libgpm
DESTDIR=$HOME/glidix make install-libgpm
make freetype
DESTDIR=$HOME/glidix make install-freetype
make libddi
DESTDIR=$HOME/glidix make install-libddi
make fstools
DESTDIR=$HOME/glidix make install-fstools
make libgwm
DESTDIR=$HOME/glidix make install-libgwm
```

## Build the ISO

Finally in `glidix-build`:

```
make
```

And the ISO file `glidix.iso` will appear.