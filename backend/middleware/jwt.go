package middleware

import (
	"context"
	"errors"
	"log"
	"net/http"
	"os"
	"strings"

	"github.com/golang-jwt/jwt/v5"
)

type contextKey string

const PublisherIDKey contextKey = "publisher_id"

var JwtSecret = []byte(getJwtSecret())

func getJwtSecret() string {
	secret := os.Getenv("JWT_SECRET")
	if secret == "" {
		log.Println("warning: JWT_SECRET is not set, using default secret")
		secret = "super-secret-key-change-in-production"
	}
	return secret
}

// Custom Claims representation
type Claims struct {
	PublisherID string `json:"publisher_id"`
	jwt.RegisteredClaims
}

// JWTMiddleware validates the bearer token and sets publisher_id context
func JWTMiddleware(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		authHeader := r.Header.Get("Authorization")
		if authHeader == "" {
			http.Error(w, `{"error": "Missing authorization header"}`, http.StatusUnauthorized)
			return
		}

		parts := strings.Split(authHeader, " ")
		if len(parts) != 2 || strings.ToLower(parts[0]) != "bearer" {
			http.Error(w, `{"error": "Invalid authorization header format (expected Bearer <token>)"}`, http.StatusUnauthorized)
			return
		}

		tokenStr := parts[1]
		claims := &Claims{}

		token, err := jwt.ParseWithClaims(tokenStr, claims, func(token *jwt.Token) (interface{}, error) {
			if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
				return nil, errors.New("unexpected signing method")
			}
			return JwtSecret, nil
		})

		if err != nil || !token.Valid {
			http.Error(w, `{"error": "Invalid or expired token"}`, http.StatusUnauthorized)
			return
		}

		// Save the publisher_id in context
		ctx := context.WithValue(r.Context(), PublisherIDKey, claims.PublisherID)
		next.ServeHTTP(w, r.WithContext(ctx))
	}
}
