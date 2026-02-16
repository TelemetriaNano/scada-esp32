// firebase-messaging-sw.js
// Service Worker para notificaciones push - SCADA NANO

importScripts('https://www.gstatic.com/firebasejs/9.22.0/firebase-app-compat.js');
importScripts('https://www.gstatic.com/firebasejs/9.22.0/firebase-messaging-compat.js');

// Configuración Firebase
firebase.initializeApp({
    apiKey: "AIzaSyCCdJ9YirX__bPZEMTP2LwhSKZl2QNCTbU",
    authDomain: "telemetria-nano-dede4.firebaseapp.com",
    projectId: "telemetria-nano-dede4",
    storageBucket: "telemetria-nano-dede4.firebasestorage.app",
    messagingSenderId: "906041908876",
    appId: "1:906041908876:web:3cc110596a45ff09e4a5a9"
});

const messaging = firebase.messaging();

// Manejar mensajes en segundo plano (cuando la pestaña está cerrada)
messaging.onBackgroundMessage((payload) => {
    console.log('[SW] Mensaje recibido en segundo plano:', payload);
    
    const notificationTitle = payload.notification.title || '🏭 SCADA NANO';
    const notificationOptions = {
        body: payload.notification.body || 'Nueva alerta del sistema',
        icon: 'data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><text y="75" font-size="75">🏭</text></svg>',
        badge: 'data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><text y="75" font-size="75">🔔</text></svg>',
        vibrate: [300, 100, 200, 100, 300],
        requireInteraction: true,
        tag: 'scada-alarm',
        data: payload.data || {}
    };
    
    return self.registration.showNotification(notificationTitle, notificationOptions);
});

// Manejar clics en las notificaciones
self.addEventListener('notificationclick', (event) => {
    console.log('[SW] Clic en notificación:', event);
    
    event.notification.close();
    
    // Abrir o enfocar el dashboard
    event.waitUntil(
        clients.matchAll({ type: 'window', includeUncontrolled: true })
            .then((clientList) => {
                // Buscar si ya hay una ventana abierta
                for (let client of clientList) {
                    if (client.url.includes('github.io') && 'focus' in client) {
                        return client.focus();
                    }
                }
                // Si no hay ventana abierta, abrir una nueva
                if (clients.openWindow) {
                    return clients.openWindow('/');
                }
            })
    );
});
