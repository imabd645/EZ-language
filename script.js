// ==========================================
// EZ Language Website - JavaScript
// ==========================================

document.addEventListener('DOMContentLoaded', () => {
    // Initialize all components
    initNavigation();
    initSmoothScroll();
    initTabs();
    initInstallTabs();
    initCopyButtons();
    initScrollAnimations();
});

// ==========================================
// Mobile Navigation Toggle
// ==========================================
function initNavigation() {
    const navToggle = document.querySelector('.nav-toggle');
    const navMenu = document.querySelector('.nav-menu');
    const navLinks = document.querySelectorAll('.nav-link');

    if (navToggle && navMenu) {
        navToggle.addEventListener('click', () => {
            navMenu.classList.toggle('active');
            navToggle.classList.toggle('active');
        });

        // Close menu when clicking a link
        navLinks.forEach(link => {
            link.addEventListener('click', () => {
                navMenu.classList.remove('active');
                navToggle.classList.remove('active');
            });
        });

        // Close menu when clicking outside
        document.addEventListener('click', (e) => {
            if (!navToggle.contains(e.target) && !navMenu.contains(e.target)) {
                navMenu.classList.remove('active');
                navToggle.classList.remove('active');
            }
        });
    }

    // Navbar background on scroll
    const navbar = document.querySelector('.navbar');
    if (navbar) {
        window.addEventListener('scroll', () => {
            if (window.scrollY > 50) {
                navbar.style.background = 'rgba(10, 10, 15, 0.95)';
            } else {
                navbar.style.background = 'rgba(10, 10, 15, 0.8)';
            }
        });
    }
}

// ==========================================
// Smooth Scrolling for Anchor Links
// ==========================================
function initSmoothScroll() {
    const anchorLinks = document.querySelectorAll('a[href^="#"]');

    anchorLinks.forEach(link => {
        link.addEventListener('click', (e) => {
            const href = link.getAttribute('href');
            if (href === '#') return;

            const target = document.querySelector(href);
            if (target) {
                e.preventDefault();
                const navHeight = document.querySelector('.navbar').offsetHeight;
                const targetPosition = target.offsetTop - navHeight;

                window.scrollTo({
                    top: targetPosition,
                    behavior: 'smooth'
                });
            }
        });
    });
}

// ==========================================
// Code Snippet Tabs
// ==========================================
function initTabs() {
    const tabButtons = document.querySelectorAll('.tab-btn');
    const tabPanels = document.querySelectorAll('.snippet-panel');

    tabButtons.forEach(button => {
        button.addEventListener('click', () => {
            const targetTab = button.dataset.tab;

            // Update active button
            tabButtons.forEach(btn => btn.classList.remove('active'));
            button.classList.add('active');

            // Update active panel
            tabPanels.forEach(panel => {
                panel.classList.remove('active');
                if (panel.id === targetTab) {
                    panel.classList.add('active');
                }
            });
        });
    });
}

// ==========================================
// Installation Tabs
// ==========================================
function initInstallTabs() {
    const installTabButtons = document.querySelectorAll('.install-tab-btn');
    const installPanels = document.querySelectorAll('.install-panel');

    installTabButtons.forEach(button => {
        button.addEventListener('click', () => {
            const targetInstall = button.dataset.install;

            // Update active button
            installTabButtons.forEach(btn => btn.classList.remove('active'));
            button.classList.add('active');

            // Update active panel
            installPanels.forEach(panel => {
                panel.classList.remove('active');
                if (panel.id === targetInstall) {
                    panel.classList.add('active');
                }
            });
        });
    });
}

// ==========================================
// Copy Code to Clipboard
// ==========================================
function initCopyButtons() {
    const copyButtons = document.querySelectorAll('.copy-btn');

    // Code snippets map
    const codeSnippets = {
        fibonacci: `// Fibonacci sequence in EZ
task fib(n) {
    when n lessthen 2 {
        give n
    }
    give fib(n - 1) + fib(n - 2)
}

// Print first 10 fibonacci numbers
repeat i = 0 to 10 {
    out fib(i)
}`,
        factorial: `// Calculate factorial
task factorial(n) {
    when n lessthen 2 {
        give 1
    }
    give n * factorial(n - 1)
}

// Get user input and calculate
out "Enter a number: "
in num
out "Factorial: " + factorial(num)`,
        conditions: `// Check if number is positive, negative, or zero
task checkNumber(n) {
    when n greater 0 {
        out "Positive!"
    } other when n less 0 {
        out "Negative!"
    } other {
        out "Zero!"
    }
}

checkNumber(42)   // Output: Positive!
checkNumber(-7)   // Output: Negative!`,
        loops: `// Different loop types in EZ

// For loop - count from 1 to 5
repeat i = 1 to 5 {
    out i
}

// While loop - run until condition is false
x = 10
until x equal 0 {
    out x
    x = x - 1
}

// Foreach - iterate over array
colors = ["red", "green", "blue"]
get color in colors {
    out color
}`
    };

    copyButtons.forEach(button => {
        button.addEventListener('click', async () => {
            const codeKey = button.dataset.code;
            const code = codeSnippets[codeKey];

            if (code) {
                try {
                    await navigator.clipboard.writeText(code);

                    // Visual feedback
                    const originalText = button.textContent;
                    button.textContent = '✓ Copied!';
                    button.style.background = 'rgba(34, 197, 94, 0.2)';
                    button.style.color = '#22c55e';

                    setTimeout(() => {
                        button.textContent = originalText;
                        button.style.background = '';
                        button.style.color = '';
                    }, 2000);
                } catch (err) {
                    console.error('Failed to copy:', err);
                    button.textContent = '✗ Error';
                    setTimeout(() => {
                        button.textContent = '📋 Copy';
                    }, 2000);
                }
            }
        });
    });
}

// ==========================================
// Scroll-based Animations
// ==========================================
function initScrollAnimations() {
    const observerOptions = {
        root: null,
        rootMargin: '0px',
        threshold: 0.1
    };

    const observer = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                entry.target.style.animationPlayState = 'running';
                observer.unobserve(entry.target);
            }
        });
    }, observerOptions);

    // Observe animated elements
    const animatedElements = document.querySelectorAll(
        '.feature-card, .keyword-item, .install-card'
    );

    animatedElements.forEach(el => {
        el.style.animationPlayState = 'paused';
        observer.observe(el);
    });
}

// ==========================================
// Active Navigation Highlighting
// ==========================================
function initActiveNavHighlight() {
    const sections = document.querySelectorAll('section[id]');
    const navLinks = document.querySelectorAll('.nav-link');

    window.addEventListener('scroll', () => {
        let current = '';
        const navHeight = document.querySelector('.navbar').offsetHeight;

        sections.forEach(section => {
            const sectionTop = section.offsetTop - navHeight - 100;
            const sectionHeight = section.offsetHeight;

            if (window.scrollY >= sectionTop && window.scrollY < sectionTop + sectionHeight) {
                current = section.getAttribute('id');
            }
        });

        navLinks.forEach(link => {
            link.classList.remove('active');
            if (link.getAttribute('href') === `#${current}`) {
                link.classList.add('active');
            }
        });
    });
}

// Initialize active nav highlighting
initActiveNavHighlight();
