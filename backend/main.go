package main

import (
	"log"
	"net/http"
	"os"

	"iot-smart-connector/backend/config"
	"iot-smart-connector/backend/handlers"
	"iot-smart-connector/backend/middleware"
)

// Simple CORS middleware wrapper for our handlers
func enableCORS(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization")

		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusOK)
			return
		}

		next(w, r)
	}
}

func main() {
	// Initialize database
	config.InitDB()

	mux := http.NewServeMux()

	// Auth Endpoints
	mux.HandleFunc("POST /api/auth/register", enableCORS(handlers.Register))
	mux.HandleFunc("POST /api/auth/login", enableCORS(handlers.Login))

	// Public Provisioning Endpoints (CORS enabled)
	mux.HandleFunc("GET /api/resolve", enableCORS(handlers.ResolveApp))
	mux.HandleFunc("POST /api/activate", enableCORS(handlers.ActivateDevice))

	// Authenticated Admin Endpoints (JWT protected)
	mux.HandleFunc("GET /api/admin/apps", enableCORS(middleware.JWTMiddleware(handlers.ListApps)))
	mux.HandleFunc("POST /api/admin/apps", enableCORS(middleware.JWTMiddleware(handlers.CreateApp)))
	mux.HandleFunc("PUT /api/admin/apps/{id}", enableCORS(middleware.JWTMiddleware(handlers.UpdateApp)))
	mux.HandleFunc("DELETE /api/admin/apps/{id}", enableCORS(middleware.JWTMiddleware(handlers.DeleteApp)))
	mux.HandleFunc("GET /api/admin/apps/{id}/activations", enableCORS(middleware.JWTMiddleware(handlers.GetAppActivations)))

	// Serve Static Admin Frontend Files with clean URLs
	mux.HandleFunc("GET /", func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/" {
			http.ServeFile(w, r, "./static/index.html")
			return
		}
		// Redirect old .html paths to clean paths
		if r.URL.Path == "/index.html" {
			http.Redirect(w, r, "/", http.StatusMovedPermanently)
			return
		}
		if r.URL.Path == "/dashboard.html" {
			http.Redirect(w, r, "/dashboard", http.StatusMovedPermanently)
			return
		}
		if r.URL.Path == "/web-ble.html" {
			http.Redirect(w, r, "/web-ble", http.StatusMovedPermanently)
			return
		}
		if r.URL.Path == "/privacy.html" {
			http.Redirect(w, r, "/privacy", http.StatusMovedPermanently)
			return
		}
		if r.URL.Path == "/terms.html" {
			http.Redirect(w, r, "/terms", http.StatusMovedPermanently)
			return
		}

		// Fallback to serving static assets (JS, CSS, images, etc.)
		http.FileServer(http.Dir("./static")).ServeHTTP(w, r)
	})

	mux.HandleFunc("GET /dashboard", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, "./static/dashboard.html")
	})

	mux.HandleFunc("GET /web-ble", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, "./static/web-ble.html")
	})

	mux.HandleFunc("GET /privacy", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, "./static/privacy.html")
	})

	mux.HandleFunc("GET /terms", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, "./static/terms.html")
	})

	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	log.Printf("TinyBeacon Server starting on port %s...", port)
	if err := http.ListenAndServe(":"+port, mux); err != nil {
		log.Fatalf("Server failed to start: %v", err)
	}
}
