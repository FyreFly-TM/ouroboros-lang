// Theme Toggle
const themeBtn = document.getElementById('theme-btn');
const body = document.body;
const icon = themeBtn.querySelector('i');

// Check for saved theme preference or default to light
const currentTheme = localStorage.getItem('theme') || 'light';
body.setAttribute('data-theme', currentTheme);
updateThemeIcon(currentTheme);

themeBtn.addEventListener('click', () => {
    const theme = body.getAttribute('data-theme') === 'light' ? 'dark' : 'light';
    body.setAttribute('data-theme', theme);
    localStorage.setItem('theme', theme);
    updateThemeIcon(theme);
});

function updateThemeIcon(theme) {
    if (theme === 'dark') {
        icon.classList.remove('fa-moon');
        icon.classList.add('fa-sun');
    } else {
        icon.classList.remove('fa-sun');
        icon.classList.add('fa-moon');
    }
}

// Smooth Scrolling Navigation
const navLinks = document.querySelectorAll('.nav-links a');
const sections = document.querySelectorAll('.section');

// Add click handler for navigation links
navLinks.forEach(link => {
    link.addEventListener('click', (e) => {
        e.preventDefault();
        const targetId = link.getAttribute('href');
        const targetSection = document.querySelector(targetId);
        
        if (targetSection) {
            const offset = 80; // Offset for fixed header
            const targetPosition = targetSection.offsetTop - offset;
            
            window.scrollTo({
                top: targetPosition,
                behavior: 'smooth'
            });
        }
        
        // Update active state
        navLinks.forEach(l => l.classList.remove('active'));
        link.classList.add('active');
    });
});

// Update active navigation on scroll
window.addEventListener('scroll', () => {
    let current = '';
    const scrollPosition = window.scrollY + 100;
    
    sections.forEach(section => {
        const sectionTop = section.offsetTop;
        const sectionHeight = section.clientHeight;
        
        if (scrollPosition >= sectionTop && scrollPosition < sectionTop + sectionHeight) {
            current = '#' + section.getAttribute('id');
        }
    });
    
    navLinks.forEach(link => {
        link.classList.remove('active');
        if (link.getAttribute('href') === current) {
            link.classList.add('active');
        }
    });
});

// Add syntax highlighting to Ouroboros code blocks
// Since Prism doesn't have Ouroboros support, we'll enhance JavaScript highlighting
document.addEventListener('DOMContentLoaded', () => {
    // Add custom keywords for Ouroboros
    Prism.languages.ouroboros = Prism.languages.extend('javascript', {
        'keyword': /\b(?:function|if|else|while|for|return|break|continue|import|export|const|let|var|class|true|false|null|undefined)\b/,
        'builtin': /\b(?:print|opengl_init|opengl_create_context|opengl_destroy_context|opengl_create_shader|opengl_use_shader|opengl_clear|opengl_draw_arrays|opengl_swap_buffers|opengl_is_context_valid|vulkan_init|vulkan_create_instance|vulkan_select_physical_device|vulkan_create_logical_device|vulkan_create_surface|vulkan_create_swapchain|vulkan_create_render_pass|vulkan_create_graphics_pipeline|vulkan_create_command_buffers|vulkan_draw_frame|vulkan_cleanup|get_input|string_length|string_concat|to_string)\b/,
        'number': /\b\d+(?:\.\d+)?\b/
    });
    
    // Apply to all code blocks
    Prism.highlightAll();
});

// Mobile Sidebar Toggle
let sidebarToggle = document.createElement('button');
sidebarToggle.className = 'sidebar-toggle';
sidebarToggle.innerHTML = '<i class="fas fa-bars"></i>';
sidebarToggle.style.cssText = `
    display: none;
    position: fixed;
    top: 1rem;
    left: 1rem;
    z-index: 101;
    background-color: var(--accent);
    color: white;
    border: none;
    padding: 0.75rem;
    border-radius: 8px;
    cursor: pointer;
    box-shadow: 0 4px 10px var(--shadow);
`;

document.body.appendChild(sidebarToggle);

const sidebar = document.querySelector('.sidebar');

sidebarToggle.addEventListener('click', () => {
    sidebar.classList.toggle('active');
});

// Show mobile toggle on small screens
function checkMobile() {
    if (window.innerWidth <= 768) {
        sidebarToggle.style.display = 'block';
    } else {
        sidebarToggle.style.display = 'none';
        sidebar.classList.remove('active');
    }
}

window.addEventListener('resize', checkMobile);
checkMobile();

// Add copy button to code blocks
document.querySelectorAll('pre').forEach(pre => {
    const button = document.createElement('button');
    button.className = 'copy-button';
    button.innerHTML = '<i class="fas fa-copy"></i>';
    button.style.cssText = `
        position: absolute;
        top: 0.5rem;
        right: 0.5rem;
        background-color: var(--accent);
        color: white;
        border: none;
        padding: 0.5rem;
        border-radius: 4px;
        cursor: pointer;
        opacity: 0;
        transition: opacity 0.3s ease;
    `;
    
    pre.style.position = 'relative';
    pre.appendChild(button);
    
    pre.addEventListener('mouseenter', () => {
        button.style.opacity = '1';
    });
    
    pre.addEventListener('mouseleave', () => {
        button.style.opacity = '0';
    });
    
    button.addEventListener('click', async () => {
        const code = pre.querySelector('code').textContent;
        try {
            await navigator.clipboard.writeText(code);
            button.innerHTML = '<i class="fas fa-check"></i>';
            setTimeout(() => {
                button.innerHTML = '<i class="fas fa-copy"></i>';
            }, 2000);
        } catch (err) {
            console.error('Failed to copy code:', err);
        }
    });
});

// Add fade-in animation on scroll
const observerOptions = {
    threshold: 0.1,
    rootMargin: '0px 0px -50px 0px'
};

const observer = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
        if (entry.isIntersecting) {
            entry.target.style.opacity = '1';
            entry.target.style.transform = 'translateY(0)';
        }
    });
}, observerOptions);

// Observe all feature cards and function docs
document.querySelectorAll('.feature-card, .function-doc, .vs-feature').forEach(el => {
    el.style.opacity = '0';
    el.style.transform = 'translateY(20px)';
    el.style.transition = 'opacity 0.6s ease, transform 0.6s ease';
    observer.observe(el);
});

// Keyboard shortcuts
document.addEventListener('keydown', (e) => {
    // Ctrl/Cmd + K for search (placeholder)
    if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
        e.preventDefault();
        alert('Search functionality coming soon!');
    }
    
    // Ctrl/Cmd + / for theme toggle
    if ((e.ctrlKey || e.metaKey) && e.key === '/') {
        e.preventDefault();
        themeBtn.click();
    }
}); 