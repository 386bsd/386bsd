#
# 386bsd kernel program.
#

HOST!=hostname -s
S=$(.CURDIR)

.if exists($(.CURDIR)/config/$(HOST).mk)
.include "$(.CURDIR)/config/$(HOST).mk"
.else
.include "$(.CURDIR)/config/stock.mk"
.endif
