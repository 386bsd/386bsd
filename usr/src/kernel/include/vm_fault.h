
/*
 * Address space fault handlers
 *	386BSD:		$Id: vm_fault.h,v 1.2 93/02/04 20:15:22 bill Exp $
 */

#ifndef VM_FAULT_H
#define VM_FAULT_H

int	vm_fault(vm_map_t map, vm_offset_t vaddr, vm_prot_t fault_type,
		boolean_t change_wiring);
void	vm_fault_wire(vm_map_t map, vm_offset_t start, vm_offset_t end);
void	vm_fault_unwire(vm_map_t map, vm_offset_t start, vm_offset_t end);
void	vm_fault_copy_entry(vm_map_t dst_map, vm_map_t src_map,
		vm_map_entry_t dst_entry, vm_map_entry_t src_entry);

#endif VM_FAULT_H
