// #include <linux/mm.h>
// #define VM_VTIME (VM_DONTEXPAND | VM_DONTDUMP)

/* .mmap           = file_mmap, \ */

// static void
// mmap_open(struct vm_area_struct *vma)
// {
//         u64 vtime = 1;
//         // printk(KERN_NOTICE "mmap vtime vma for %s\n", current->comm);

//         current->vtime_vma = (void *)get_zeroed_page(GFP_KERNEL);

//         // Initialize with a vtime spd of 1     
//         memcpy(current->vtime_vma, &vtime, sizeof(u64));

//         // printk(KERN_NOTICE "\tinit val is %llu\n", *(u64 *)current->vtime_vma);
// }

// static void
// mmap_close(struct vm_area_struct *vma)
// {
//         free_page((unsigned long)current->vtime_vma);
// }

// static int
// mmap_fault(struct vm_fault *vmf)
// {
//         struct page *vtime_page;
//         // u64 *vma = current->vtime_vma;

//         // printk(KERN_NOTICE "mmap fault handler\n");
//         // printk(KERN_NOTICE "\tfault addr %lu\n", vmf->address);
//         // printk(KERN_NOTICE "\tfault flags %u\n", vmf->flags);
//         // printk(KERN_NOTICE "\tvtime_vma addr %lu\n", (unsigned long)current->vtime_vma);
        
//         if (!current->vtime_vma)
//                 return VM_FAULT_SIGBUS;

//         // while (*vma) {
//         //      printk(KERN_NOTICE "\tvma value %llu\n", *vma);
//         //      vma++;
//         // }

//         vtime_page = virt_to_page(current->vtime_vma);
        
//         get_page(vtime_page);
//         vmf->page = vtime_page;
//         return 0;
// }


// struct vm_operations_struct tense_vmops = {
//         .open =         mmap_open,
//         .close =        mmap_close,
//         .fault =        mmap_fault,
// };

// static int
// file_mmap(struct file *filp, struct vm_area_struct *vma)
// {
//         printk(KERN_NOTICE "file mmap %s\n", current->comm);

//         vma->vm_ops = &tense_vmops;
//         vma->vm_flags |= VM_VTIME;
//         mmap_open(vma);
//         return 0;
// }
