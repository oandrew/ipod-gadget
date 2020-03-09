VERSION ?= 0.1

.PHONY: dkms
dkms:
	dkms add -m ipod-gadget/$(VERSION)
	dkms build -m ipod-gadget/$(VERSION)
	dkms install -m ipod-gadget/$(VERSION)
	cp ipod-modules.conf /etc/modules-load.d/

.PHONY: uninstall
uninstall:
	dkms remove -m ipod-gadget/$(VERSION) --all
	rm /etc/modules-load.d/ipod-modules.conf
